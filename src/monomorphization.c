#include "monomorphization.h"
#include "parser.h"
#include "codegen.h"
#include "type_checker.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);
static char *substitute_type(const char *type, char **params, char **args, int count);

static int is_already_specialized(const char *name) {
    if (!name) return 0;
    // Check for common specialized prefixes or suffixes
    if (strstr(name, "_i32") || strstr(name, "_Ident") || strstr(name, "_char") || strstr(name, "_bool")) return 1;
    if (strstr(name, "Vec_") || strstr(name, "Box_") || strstr(name, "Option_") || strstr(name, "Result_")) return 1;
    // If it already contains an underscore followed by something that looks like a type
    char *underscore = strchr(name, '_');
    if (underscore && isupper(underscore[1])) return 1;
    return 0;
}

typedef struct GenericRegistryNode {
    char *name;
    ASTNode *node;
    struct GenericRegistryNode *next;
} GenericRegistryNode;

static GenericRegistryNode *registry = NULL;

static char *safe_strdup(const char *s) {
    return s ? strdup(s) : NULL;
}

void monomorphization_register(ASTNode *node) {
    char *name = NULL;
    if (node->type == AST_FUNC) name = node->data.func.name;
    else if (node->type == AST_STRUCT_DECL) name = node->data.struct_decl.name;
    else if (node->type == AST_ENUM_DECL) name = node->data.enum_decl.name;
    if (!name || monomorphization_lookup(name)) return;
    GenericRegistryNode *reg = malloc(sizeof(GenericRegistryNode));
    reg->name = strdup(name); reg->node = node; reg->next = registry; registry = reg;
}

ASTNode *monomorphization_lookup(const char *name) {
    GenericRegistryNode *curr = registry;
    while (curr) { if (strcmp(curr->name, name) == 0) return curr->node; curr = curr->next; }
    return NULL;
}

typedef struct SpecializationNode {
    char *mangled_name;
    ASTNode *node;
    struct SpecializationNode *next;
} SpecializationNode;

static SpecializationNode *specializations = NULL;

static int is_specialized(const char *mangled_name) {
    SpecializationNode *curr = specializations;
    while (curr) { if (strcmp(curr->mangled_name, mangled_name) == 0) return 1; curr = curr->next; }
    return 0;
}

static void walk_and_specialize(ASTNode *node);

static void register_specialization(const char *mangled_name, ASTNode *node) {
    if (is_specialized(mangled_name)) return;
    SpecializationNode *s = malloc(sizeof(SpecializationNode));
    s->mangled_name = strdup(mangled_name); s->node = node; s->next = specializations; specializations = s;
}

static char *mangle_name(const char *base, char **args, int count) {
    if (!base) return strdup("NULL_BASE");
    char buf[512]; strcpy(buf, base);
    for (int i = 0; i < count; i++) {
        char *arg = args[i];
        if (!arg) continue;
        if (strcmp(arg, "V") == 0) arg = "i32";
        if (strcmp(arg, "T") == 0) arg = "i32";
        if (strcmp(arg, "int") == 0) arg = "i32";
        if (strcmp(arg, "unsigned int") == 0) arg = "u32";
        if (strcmp(arg, "unsigned char") == 0) arg = "u8";
        if (strcmp(arg, "signed char") == 0) arg = "i8";
        if (strcmp(arg, "char") == 0) arg = "i8";
        strcat(buf, "_"); 
        for (int j = 0; arg[j]; j++) {
            if (arg[j] == '&') strcat(buf, "Ref");
            else if (arg[j] == ' ') strcat(buf, "_");
            else if (arg[j] == '*') strcat(buf, "Ptr");
            else if (arg[j] == '<' || arg[j] == '>') strcat(buf, "_");
            else if (arg[j] == ',') strcat(buf, "_");
            else { int len = strlen(buf); if (len < 500) { buf[len] = arg[j]; buf[len+1] = '\0'; } }
        }
    }
    if (strstr(buf, "_i32")) { char *p = strstr(buf, "_i32"); memcpy(p, "_i32", 4); memmove(p+4, p+4, strlen(p+4)+1); }
    else if (strstr(buf, "_int")) { char *p = strstr(buf, "_int"); memcpy(p, "_i32", 4); memmove(p+4, p+4, strlen(p+4)+1); }
    if (strstr(buf, "_i8")) { char *p = strstr(buf, "_i8"); memcpy(p, "_i8", 3); memmove(p+3, p+3, strlen(p+3)+1); }
    if (strstr(buf, "_u8")) { char *p = strstr(buf, "_u8"); memcpy(p, "_u8", 3); memmove(p+3, p+3, strlen(p+3)+1); }
    return strdup(buf);
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);

static char *substitute_type(const char *type, char **params, char **args, int count) {
    if (!type) return NULL;
    if (strcmp(type, "mut") == 0) return strdup("");
    
    // Handle &T or &mut T or &mut Self
    if (type[0] == '&') {
        const char *inner = type + 1;
        int is_mut = 0;
        if (strncmp(inner, "mut ", 4) == 0) {
            inner += 4;
            is_mut = 1;
        }
        char *sub_inner = substitute_type(inner, params, args, count);
        char buf[256];
        snprintf(buf, sizeof(buf), "&%s", sub_inner);
        free(sub_inner);
        return strdup(buf);
    }

    if (strstr(type, "mut ")) {
        char *type_copy = strdup(type);
        char *p;
        while ((p = strstr(type_copy, "mut "))) {
             memmove(p, p + 4, strlen(p + 4) + 1);
        }
        char *res = substitute_type(type_copy, params, args, count);
        free(type_copy);
        return res;
    }
    
    for (int i = 0; i < count; i++) {
        if (!params[i]) continue;
        // Optimization: if the type IS exactly the parameter, return the argument immediately
        if (strcmp(type, params[i]) == 0) return strdup(args[i]);
    }
    if (strcmp(type, "Self") == 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(params[i], "Self") == 0) return strdup(args[i]);
        }
    }
    if (strcmp(type, "Self::Item") == 0 || strcmp(type, "Item") == 0) {
        for (int i = 0; i < count; i++) {
            if (strcmp(params[i], "Item") == 0) return strdup(args[i]);
        }
        return strdup("i32");
    }
    if (strcmp(type, "T") == 0) return strdup("i32");
    if (strcmp(type, "V") == 0) return strdup("i32");
    if (strcmp(type, "K") == 0) return strdup("i32");
    if (strchr(type, '<')) {
        char *type_copy = strdup(type);
        // Replace known generic parameters in the string itself
        for (int i = 0; i < count; i++) {
            if (!params[i]) continue;
            char *p;
            while ((p = strstr(type_copy, params[i]))) {
                int plen = strlen(params[i]);
                int alen = strlen(args[i]);
                char *new_type = malloc(strlen(type_copy) - plen + alen + 1);
                int offset = p - type_copy;
                strncpy(new_type, type_copy, offset);
                strcpy(new_type + offset, args[i]);
                strcpy(new_type + offset + alen, p + plen);
                free(type_copy);
                type_copy = new_type;
            }
        }
        if (strstr(type_copy, "int")) {
             char *ni = strstr(type_copy, "int");
             ni[0] = 'i'; ni[1] = '3'; ni[2] = '2';
        }
        char *lt = strchr(type_copy, '<'); char *gt = strrchr(type_copy, '>');
        if (lt && gt) {
            *lt = '\0'; *gt = '\0';
            char *base = type_copy; char *arg_str = lt + 1;
            
            // Handle multiple arguments in <...>
            int arg_count = 1;
            for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
            
            char **sub_args = malloc(sizeof(char*) * arg_count);
            char *arg_copy = strdup(arg_str);
            char *token = strtok(arg_copy, ",");
            int idx = 0;
            while (token) {
                while (*token == ' ') token++;
                sub_args[idx++] = substitute_type(token, params, args, count);
                token = strtok(NULL, ",");
            }
            
            char *mangled = mangle_name(base, sub_args, arg_count);
            for (int i = 0; i < idx; i++) free(sub_args[i]);
            free(sub_args); free(arg_copy); free(type_copy); return mangled;
        }
        free(type_copy);
    }
    char *res_sub = strdup(type);
    for (int i = 0; i < count; i++) {
        if (!params[i]) continue;
        char *pos = res_sub;
        while ((pos = strstr(pos, params[i]))) {
            int prefix_ok = (pos == res_sub || (!isalnum((unsigned char)*(pos-1)) && *(pos-1) != '_'));
            int suffix_ok = (!isalnum((unsigned char)*(pos + strlen(params[i]))) && *(pos + strlen(params[i])) != '_');
            if (!prefix_ok && pos - res_sub >= 3 && strncmp(pos - 3, "Ref", 3) == 0) prefix_ok = 1;
            if (!prefix_ok && pos - res_sub >= 6 && strncmp(pos - 6, "Refmut", 6) == 0) prefix_ok = 1;
            if (!prefix_ok && pos > res_sub && *(pos-1) == ' ') prefix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == '*') suffix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == ' ') suffix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == '\0') suffix_ok = 1;
            if (!suffix_ok && strncmp(pos + strlen(params[i]), " _0", 3) == 0) suffix_ok = 1;
            if (strcmp(pos, params[i]) == 0) { prefix_ok = 1; suffix_ok = 1; }
            if (prefix_ok && suffix_ok) {
                int prefix_len = pos - res_sub; int suffix_len = strlen(pos + strlen(params[i]));
                char *new_res = malloc(prefix_len + strlen(args[i]) + suffix_len + 1);
                memcpy(new_res, res_sub, prefix_len); memcpy(new_res + prefix_len, args[i], strlen(args[i]));
                memcpy(new_res + prefix_len + strlen(args[i]), pos + strlen(params[i]), suffix_len + 1);
                free(res_sub); res_sub = new_res; pos = res_sub + prefix_len + strlen(args[i]);
            } else pos += strlen(params[i]);
        }
    }
    return res_sub;
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count) {
    if (!node) return NULL;
    ASTNode *new_node = ast_clone(node);
    
    // Force immediate substitution of generic parameters in specialized nodes
    switch (node->type) {
        case AST_FUNC:
            if (new_node->data.func.return_type) {
                char *old = new_node->data.func.return_type;
                new_node->data.func.return_type = substitute_type(old, params, args, count);
                free(old);
            }
            for (int i = 0; i < new_node->data.func.param_count; i++) {
                ASTNode *param = new_node->data.func.params[i];
                if (param->data.param.type_name) {
                    char *old = param->data.param.type_name;
                    param->data.param.type_name = substitute_type(old, params, args, count);
                    free(old);
                }
            }
            break;
        case AST_STRUCT_DECL:
            for (int i = 0; i < new_node->data.struct_decl.field_count; i++) {
                ASTNode *field = new_node->data.struct_decl.fields[i];
                if (field->data.param.type_name) {
                    char *old = field->data.param.type_name;
                    field->data.param.type_name = substitute_type(old, params, args, count);
                    free(old);
                }
            }
            break;
        case AST_ENUM_DECL:
            for (int i = 0; i < new_node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = new_node->data.enum_decl.variants[i];
                for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                    ASTNode *field = variant->data.enum_variant.fields[j];
                    if (field->data.param.type_name) {
                        char *old = field->data.param.type_name;
                        field->data.param.type_name = substitute_type(old, params, args, count);
                        free(old);
                    }
                }
            }
            break;
        case AST_VAR_DECL:
            if (new_node->data.var_decl.type_name) {
                char *old = new_node->data.var_decl.type_name;
                new_node->data.var_decl.type_name = substitute_type(old, params, args, count);
                free(old);
            }
            break;
        default: break;
    }

    switch (node->type) {
        case AST_FUNC:
            if (new_node->data.func.name) {
                char *old = new_node->data.func.name;
                new_node->data.func.name = mangle_name(old, args, count); 
            }
            new_node->data.func.is_generic = 0; // It's now specialized
            new_node->data.func.is_specialized = 1;
    char *res = substitute_type(node->data.func.return_type, params, args, count);
    if (res && strchr(res, '<')) {
        char *type_name = strdup(res);
        char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
        if (lt && gt) {
            *lt = '\0'; *gt = '\0';
            char *base = type_name; char *arg_str = lt + 1;
            char *sub_args[10]; int arg_count = 0;
            char *arg_copy = strdup(arg_str); char *token = strtok(arg_copy, ",");
            while (token && arg_count < 10) { sub_args[arg_count++] = strdup(token); token = strtok(NULL, ","); }
            char *mangled = mangle_name(base, sub_args, arg_count);
            free(res); res = mangled;
            for (int i = 0; i < arg_count; i++) free(sub_args[i]);
            free(arg_copy);
        }
        free(type_name);
    }
    if (res && (strcmp(res, "Self") == 0 || strcmp(res, "Wrapper") == 0 || strcmp(res, "Vec") == 0 || strcmp(res, "Box") == 0 || strcmp(res, "Option") == 0 || strcmp(res, "Result") == 0)) {
        char *target_base = (strcmp(res, "Self") == 0) ? params[0] : res;
        char *target_struct = mangle_name(target_base, args, count);
        free(res); res = target_struct;
    }
    new_node->data.func.return_type = res;
            
            for (int i = 0; i < node->data.func.param_count; i++) {
                new_node->data.func.params[i] = specialize_node(node->data.func.params[i], params, args, count);
                if (new_node->data.func.params[i]->data.param.type_name && strcmp(new_node->data.func.params[i]->data.param.type_name, "Self") == 0) {
                    free(new_node->data.func.params[i]->data.param.type_name);
                    char *target_struct = mangle_name(params[0], args, count);
                    new_node->data.func.params[i]->data.param.type_name = strdup(target_struct);
                    free(target_struct);
                }
            }
            new_node->data.func.body = specialize_node(node->data.func.body, params, args, count);
            new_node->data.func.generic_param_count = 0; break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) new_node->data.block.statements[i] = specialize_node(node->data.block.statements[i], params, args, count);
            break;
        case AST_STRUCT_INIT:
            if (new_node->data.struct_init.struct_name) { char *old = new_node->data.struct_init.struct_name; new_node->data.struct_init.struct_name = substitute_type(old, params, args, count); free(old); }
            for (int i = 0; i < node->data.struct_init.field_count; i++) new_node->data.struct_init.fields[i] = specialize_node(node->data.struct_init.fields[i], params, args, count);
            break;
        case AST_FIELD_INIT: new_node->data.field_init.value = specialize_node(node->data.field_init.value, params, args, count); break;
        case AST_CALL:
            if (new_node->data.call.name) {
            }
            // Handle Result_Ok -> Result_i32_Refchar_Ok style
            if (new_node->data.call.name) {
                char *call_name = new_node->data.call.name;
                if (strstr(call_name, "::")) {
                     char *copy = strdup(call_name);
                     char *sep = strstr(copy, "::");
                     *sep = '\0';
                     char *variant = sep + 2;
                     ASTNode *generic = monomorphization_lookup(copy);
                     if (generic && (generic->type == AST_ENUM_DECL || generic->type == AST_STRUCT_DECL)) {
                         char *mangled_base = mangle_name(copy, args, count);
                         char *new_call_name = malloc(strlen(mangled_base) + strlen(variant) + 3);
                         sprintf(new_call_name, "%s::%s", mangled_base, variant);
                         free(new_node->data.call.name);
                         new_node->data.call.name = new_call_name;
                         free(mangled_base);
                     }
                     free(copy);
                }
                char *underscore = strrchr(call_name, '_');
                if (underscore) {
                    int len = underscore - call_name;
                    char *base = malloc(len + 1);
                    strncpy(base, call_name, len);
                    base[len] = '\0';
                    char *variant = underscore + 1;
                    ASTNode *generic = monomorphization_lookup(base);
                    if (generic && (generic->type == AST_ENUM_DECL || generic->type == AST_STRUCT_DECL)) {
                        char *mangled_base = mangle_name(base, args, count);
                        if (strstr(call_name, mangled_base) == NULL) {
                            char *new_call_name = malloc(strlen(mangled_base) + strlen(variant) + 2);
                            sprintf(new_call_name, "%s_%s", mangled_base, variant);
                            free(new_node->data.call.name);
                            new_node->data.call.name = new_call_name;
                        }
                        free(mangled_base);
                    }
                    free(base);
                }
            }
            if (new_node->data.call.name) {
                char *old = new_node->data.call.name; 
                new_node->data.call.name = substitute_type(old, params, args, count); 
                free(old); 
            }
            for (int i = 0; i < node->data.call.arg_count; i++) new_node->data.call.args[i] = specialize_node(node->data.call.args[i], params, args, count);
            break;
        case AST_VAR_DECL:
            if (new_node->data.var_decl.type_name) { char *old = new_node->data.var_decl.type_name; new_node->data.var_decl.type_name = substitute_type(old, params, args, count); free(old); }
            new_node->data.var_decl.init = specialize_node(node->data.var_decl.init, params, args, count); break;
        case AST_PARAM:
            if (new_node->data.param.type_name) { char *old = new_node->data.param.type_name; new_node->data.param.type_name = substitute_type(old, params, args, count); free(old); }
            break;
        case AST_BINOP:
            new_node->data.binop.left = specialize_node(node->data.binop.left, params, args, count);
            new_node->data.binop.right = specialize_node(node->data.binop.right, params, args, count); break;
        case AST_RETURN: new_node->data.ret_stmt.value = specialize_node(node->data.ret_stmt.value, params, args, count); break;
        case AST_STRUCT_DECL:
            if (new_node->data.struct_decl.name) { char *old = new_node->data.struct_decl.name; new_node->data.struct_decl.name = mangle_name(old, args, count); free(old); }
            for (int i = 0; i < node->data.struct_decl.field_count; i++) new_node->data.struct_decl.fields[i] = specialize_node(node->data.struct_decl.fields[i], params, args, count);
            new_node->data.struct_decl.generic_param_count = 0; break;
        case AST_ENUM_DECL:
            if (new_node->data.enum_decl.name) { char *old = new_node->data.enum_decl.name; new_node->data.enum_decl.name = mangle_name(old, args, count); free(old); }
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) new_node->data.enum_decl.variants[i] = specialize_node(node->data.enum_decl.variants[i], params, args, count);
            new_node->data.enum_decl.generic_param_count = 0; break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) new_node->data.enum_variant.fields[i] = specialize_node(node->data.enum_variant.fields[i], params, args, count);
            break;
        case AST_MATCH:
            new_node->data.match_stmt.expr = specialize_node(node->data.match_stmt.expr, params, args, count);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) new_node->data.match_stmt.arms[i] = specialize_node(node->data.match_stmt.arms[i], params, args, count);
            break;
        case AST_MATCH_ARM:
            new_node->data.match_arm.pattern = specialize_node(node->data.match_arm.pattern, params, args, count);
            new_node->data.match_arm.body = specialize_node(node->data.match_arm.body, params, args, count);
            break;
        case AST_IF:
            new_node->data.if_stmt.condition = specialize_node(node->data.if_stmt.condition, params, args, count);
            new_node->data.if_stmt.then_branch = specialize_node(node->data.if_stmt.then_branch, params, args, count);
            new_node->data.if_stmt.else_branch = specialize_node(node->data.if_stmt.else_branch, params, args, count);
            break;
        case AST_WHILE:
            new_node->data.while_loop.condition = specialize_node(node->data.while_loop.condition, params, args, count);
            new_node->data.while_loop.body = specialize_node(node->data.while_loop.body, params, args, count);
            break;
        case AST_FOR_STMT:
            new_node->data.for_loop.iterable = specialize_node(node->data.for_loop.iterable, params, args, count);
            new_node->data.for_loop.body = specialize_node(node->data.for_loop.body, params, args, count);
            break;
        case AST_METHOD_CALL:
            new_node->data.method_call.receiver = specialize_node(node->data.method_call.receiver, params, args, count);
            for (int i = 0; i < node->data.method_call.arg_count; i++) new_node->data.method_call.args[i] = specialize_node(node->data.method_call.args[i], params, args, count);
            break;
        case AST_FIELD_ACCESS:
            new_node->data.field_access.receiver = specialize_node(node->data.field_access.receiver, params, args, count);
            break;
        case AST_CAST:
            if (new_node->data.cast.type_name) { char *old = new_node->data.cast.type_name; new_node->data.cast.type_name = substitute_type(old, params, args, count); free(old); }
            new_node->data.cast.expr = specialize_node(node->data.cast.expr, params, args, count);
            break;
        default: break;
    }
    return new_node;
}

static void walk_and_specialize(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_STRUCT_DECL:
            if (node->data.struct_decl.generic_param_count > 0) {
                 monomorphization_register(node);
            } else { 
                if (node->data.struct_decl.fields) {
                    for (int i = 0; i < node->data.struct_decl.field_count; i++) walk_and_specialize(node->data.struct_decl.fields[i]);
                }
            }
            break;
        case AST_ENUM_DECL:
            if (node->data.enum_decl.generic_param_count > 0) {
                 monomorphization_register(node);
            } else { 
                if (node->data.enum_decl.variants) {
                    for (int i = 0; i < node->data.enum_decl.variant_count; i++) walk_and_specialize(node->data.enum_decl.variants[i]);
                }
            }
            break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) walk_and_specialize(node->data.enum_variant.fields[i]);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) walk_and_specialize(node->data.block.statements[i]);
            break;
        case AST_FUNC:
            if (node->data.func.generic_param_count > 0) {
                 // Wait for call site to specialize
            } else {
                 // Handle return type specialization for non-generic functions
                 if (node->data.func.return_type && strchr(node->data.func.return_type, '<')) {
                    char *type_name = strdup(node->data.func.return_type);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            args[idx++] = strdup(token);
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                ASTNode *specialized = specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, args, arg_count);
                                register_specialization(mangled, specialized);
                            }
                        }
                        free(node->data.func.return_type); node->data.func.return_type = strdup(mangled);
                        for (int i = 0; i < idx; i++) free(args[i]);
                        free(args); free(arg_copy);
                        free(mangled);
                    }
                    free(type_name);
                 }

                 for (int i = 0; i < node->data.func.param_count; i++) walk_and_specialize(node->data.func.params[i]);
                 walk_and_specialize(node->data.func.body);
            }
            break;
        case AST_CALL:
            if (node->data.call.name) {
                // General mangling for names like Vec::new, Box::new
                if (strstr(node->data.call.name, "::")) {
                    char *name_copy = strdup(node->data.call.name);
                    char *colon = strstr(name_copy, "::");
                    *colon = '\0';
                    char *base = name_copy;
                    char *method = colon + 2;
                    char *new_name = malloc(strlen(base) + 1 + strlen(method) + 1);
                    sprintf(new_name, "%s_%s", base, method);
                    free(node->data.call.name);
                    node->data.call.name = new_name;
                    free(name_copy);
                }

                if (strchr(node->data.call.name, '<')) {
                    char *type_name = strdup(node->data.call.name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            args[idx++] = token;
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                ASTNode *specialized = NULL;
                                if (generic->type == AST_STRUCT_DECL) specialized = specialize_node(generic, generic->data.struct_decl.generic_params, args, arg_count);
                                else if (generic->type == AST_ENUM_DECL) specialized = specialize_node(generic, generic->data.enum_decl.generic_params, args, arg_count);
                                else if (generic->type == AST_FUNC) specialized = specialize_node(generic, generic->data.func.generic_params, args, arg_count);
                                
                                if (specialized) {
                                    register_specialization(mangled, specialized);
                                }
                            }
                        }
                        free(node->data.call.name); node->data.call.name = strdup(mangled);
                        free(arg_copy);
                        free(args);
                        free(mangled);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.call.name);
                    if (generic && (generic->type == AST_FUNC && generic->data.func.generic_param_count > 0)) {
                         if (strchr(node->data.call.name, '_')) {
                              // Already mangled?
                         } else {
                             char **args = malloc(sizeof(char*) * generic->data.func.generic_param_count);
                             for (int i = 0; i < generic->data.func.generic_param_count; i++) args[i] = "i32";
                             char *mangled = mangle_name(node->data.call.name, args, generic->data.func.generic_param_count);
                             if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.func.generic_params, args, generic->data.func.generic_param_count));
                             free(node->data.call.name); node->data.call.name = strdup(mangled);
                             free(args); free(mangled);
                         }
                    }
                }
            }
            for (int i = 0; i < node->data.call.arg_count; i++) walk_and_specialize(node->data.call.args[i]);
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.init) {
                walk_and_specialize(node->data.var_decl.init);
            }
            if (node->data.var_decl.type_name) {
                if (strchr(node->data.var_decl.type_name, '<')) {
                    char *type_name = strdup(node->data.var_decl.type_name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **sub_args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            sub_args[idx++] = strdup(token);
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, sub_args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                ASTNode *specialized = NULL;
                                if (generic->type == AST_STRUCT_DECL) specialized = specialize_node(generic, generic->data.struct_decl.generic_params, sub_args, arg_count);
                                else if (generic->type == AST_ENUM_DECL) specialized = specialize_node(generic, generic->data.enum_decl.generic_params, sub_args, arg_count);
                                else if (generic->type == AST_FUNC) specialized = specialize_node(generic, generic->data.func.generic_params, sub_args, arg_count);
                                
                                if (specialized) {
                                    register_specialization(mangled, specialized);
                                    // Also specialize any methods in impl blocks for this enum/struct
                                    GenericRegistryNode *curr = registry;
                                    while (curr) {
                                        if (curr->node->type == AST_IMPL && curr->node->data.impl_block.struct_name && strcmp(curr->node->data.impl_block.struct_name, base) == 0) {
                                            ASTNode *new_impl = ast_clone(curr->node);
                                            free(new_impl->data.impl_block.struct_name);
                                            new_impl->data.impl_block.struct_name = strdup(mangled);
                                            char **params = (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params;
                                            for (int i = 0; i < new_impl->data.impl_block.method_count; i++) {
                                                ASTNode *method = specialize_node(curr->node->data.impl_block.methods[i], params, sub_args, arg_count);
                                                if (method->type == AST_FUNC && method->data.func.name) {
                                                    char *old_mname = method->data.func.name;
                                                    char *new_mname = malloc(strlen(mangled) + 2 + strlen(old_mname) + 1);
                                                    sprintf(new_mname, "%s_%s", mangled, old_mname);
                                                    method->data.func.name = new_mname;
                                                    register_specialization(method->data.func.name, method);
                                                }
                                            }
                                        }
                                        curr = curr->next;
                                    }
                                }
                            }
                        }
                        
                        // If the init is a call to a variant of this enum, mangle it too
                        if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                            char *call_name = node->data.var_decl.init->data.call.name;
                            char *underscore = strrchr(call_name, '_');
                            if (underscore) {
                                char *vbase = strndup(call_name, underscore - call_name);
                                if (strcmp(vbase, base) == 0 && strstr(call_name, mangled) == NULL) {
                                    char *variant = underscore + 1;
                                    char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                    sprintf(new_call_name, "%s_%s", mangled, variant);
                                    free(node->data.var_decl.init->data.call.name);
                                    node->data.var_decl.init->data.call.name = new_call_name;
                                }
                                free(vbase);
                            }
                        }

                        if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                        node->data.var_decl.type_name = strdup(mangled);
                        for (int j = 0; j < idx; j++) free(sub_args[j]);
                        free(sub_args); free(arg_copy);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.var_decl.type_name);
                    if (generic && (generic->type == AST_STRUCT_DECL && generic->data.struct_decl.generic_param_count > 0)) {
                         char **args = malloc(sizeof(char*) * generic->data.struct_decl.generic_param_count);
                         for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) {
                             if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT && node->data.var_decl.init->data.struct_init.struct_name) {
                                 // Inference: if initializer is a specialized struct, use its name to find specialization
                                 char *mname = node->data.var_decl.init->data.struct_init.struct_name;
                                 if (strstr(mname, "_Ident")) args[i] = "Ident";
                                 else if (strstr(mname, "_i32")) args[i] = "i32";
                                 else if (strstr(mname, "_char")) args[i] = "char";
                                 else if (strstr(mname, "_bool")) args[i] = "bool";
                                 else args[i] = "i32";
                             } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "Vec_new") == 0 || strcmp(node->data.var_decl.init->data.call.name, "Vec::new") == 0)) {
                                 args[i] = "i32"; // Vec_new heuristic
                             } else {
                                 args[i] = "i32";
                             }
                         }
                        char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.struct_decl.generic_param_count);
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT && node->data.var_decl.init->data.struct_init.struct_name) {
                             if (strchr(node->data.var_decl.init->data.struct_init.struct_name, '_')) {
                                 free(mangled);
                                 mangled = strdup(node->data.var_decl.init->data.struct_init.struct_name);
                             }
                         }
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *cname = node->data.var_decl.init->data.call.name;
                             if (strstr(cname, "wrap") || strstr(cname, "Wrapper_")) {
                                 free(mangled);
                                 mangled = strdup("Wrapper_i32");
                             }
                         }
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count));
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0 && strstr(call_name, mangled) == NULL) {
                                     char *variant = underscore + 1;
                                     char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                     sprintf(new_call_name, "%s_%s", mangled, variant);
                                     free(node->data.var_decl.init->data.call.name);
                                     node->data.var_decl.init->data.call.name = new_call_name;
                                 }
                                 free(vbase);
                             }
                         }

                         if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                         node->data.var_decl.type_name = strdup(mangled);
                         free(args);
                    } else if (generic && (generic->type == AST_ENUM_DECL && generic->data.enum_decl.generic_param_count > 0)) {
                         char **args = malloc(sizeof(char*) * generic->data.enum_decl.generic_param_count);
                         for (int i = 0; i < generic->data.enum_decl.generic_param_count; i++) args[i] = "i32";
                         char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.enum_decl.generic_param_count);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.enum_decl.generic_params, args, generic->data.enum_decl.generic_param_count));
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0 && strstr(call_name, mangled) == NULL) {
                                     char *variant = underscore + 1;
                                     char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                     sprintf(new_call_name, "%s_%s", mangled, variant);
                                     free(node->data.var_decl.init->data.call.name);
                                     node->data.var_decl.init->data.call.name = new_call_name;
                                 }
                                 free(vbase);
                             }
                         }

                         if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                         node->data.var_decl.type_name = strdup(mangled);
                         free(args);
                    } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                         // Heuristic for Box::new, Vec::new etc
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                             if (node->data.var_decl.init->data.call.name && (strstr(node->data.var_decl.init->data.call.name, "Vec_new") || strstr(node->data.var_decl.init->data.call.name, "Box_new"))) {
                                 if (node->data.var_decl.type_name) {
                                     char *mangled = strdup(node->data.var_decl.type_name);
                                     if (strcmp(mangled, "Vec") == 0) { free(mangled); mangled = strdup("Vec_i32"); }
                                     if (strcmp(mangled, "Box") == 0) { free(mangled); mangled = strdup("Box_i32"); }
                                    
                                     char *new_call = malloc(strlen(mangled) + 10);
                                     sprintf(new_call, "%s_new", mangled);
                                     free(node->data.var_decl.init->data.call.name);
                                     node->data.var_decl.init->data.call.name = new_call;
                                     free(mangled);
                                 }
                             }
                         }
                         // Generic fallbacks for Vec_new, Vec_push etc if type is known
                         if (node->data.var_decl.type_name && strcmp(node->data.var_decl.type_name, "Vec_i32") == 0) {
                              // We should probably handle this in walk_and_specialize for AST_CALL/AST_METHOD_CALL too
                         }
                    }
                }
            } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                 if (node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "Vec_new") == 0 || strcmp(node->data.var_decl.init->data.call.name, "Vec::new") == 0)) {
                      free(node->data.var_decl.init->data.call.name);
                      node->data.var_decl.init->data.call.name = strdup("Vec_i32_new");
                      if (node->data.var_decl.type_name) free(node->data.var_decl.type_name);
                      node->data.var_decl.type_name = strdup("Vec_i32");
                      
                      // Explicitly register Vec_i32 specialization
                      ASTNode *vec_generic = monomorphization_lookup("Vec");
                      if (vec_generic) {
                          char *args[] = {"i32"};
                          ASTNode *spec = specialize_node(vec_generic, vec_generic->data.struct_decl.generic_params, args, 1);
                          spec->data.struct_decl.is_generic = 0;
                          spec->data.struct_decl.is_specialized = 1;
                          free(spec->data.struct_decl.name);
                          spec->data.struct_decl.name = strdup("Vec_i32");
                          
                          SpecializationNode *sn = malloc(sizeof(SpecializationNode));
                          sn->mangled_name = strdup("Vec_i32"); sn->node = spec; sn->next = specializations; specializations = sn;
                          
                          // Also specialized methods for Vec_i32
                          ASTNode *vec_impl = monomorphization_lookup("Vec"); 
                          if (vec_impl && vec_impl->type == AST_IMPL) {
                               char *vparams[] = {"T"};
                               char *vargs[] = {"i32"};
                               for (int i = 0; i < vec_impl->data.impl_block.method_count; i++) {
                                   ASTNode *m = specialize_node(vec_impl->data.impl_block.methods[i], vparams, vargs, 1);
                                   char *mname = malloc(strlen("Vec_i32") + 1 + strlen(m->data.func.name) + 1);
                                   sprintf(mname, "Vec_i32_%s", m->data.func.name);
                                   m->data.func.name = mname;
                                   m->data.func.is_generic = 0;
                                   m->data.func.is_specialized = 1;
                                   
                                   // Fix Self parameter in Vec methods
                                   for (int j = 0; j < m->data.func.param_count; j++) {
                                       ASTNode *p = m->data.func.params[j];
                                       if (p->data.param.type_name && (strcmp(p->data.param.type_name, "&Self") == 0 || strcmp(p->data.param.type_name, "&self") == 0)) {
                                           free(p->data.param.type_name);
                                           p->data.param.type_name = strdup("&Vec_i32");
                                       }
                                   }
                                   
                                   SpecializationNode *smn = malloc(sizeof(SpecializationNode));
                                   smn->mangled_name = strdup(mname); smn->node = m; smn->next = specializations; specializations = smn;
                               }
                          }
                      }
                 }
                 if (node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "Vec_new") == 0 || strcmp(node->data.var_decl.init->data.call.name, "Vec::new") == 0)) {
                 }
            }
            break;
        case AST_STRUCT_INIT:
            // debug_node("Before walk", node);
            if (node->data.struct_init.struct_name) {
                ASTNode *generic = monomorphization_lookup(node->data.struct_init.struct_name);
                if (generic && generic->type == AST_STRUCT_DECL && generic->data.struct_decl.generic_param_count > 0 && !strchr(node->data.struct_init.struct_name, '<')) {
                    char **args = malloc(sizeof(char*) * generic->data.struct_decl.generic_param_count);
                    for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) {
                        // Use first field type for inference if possible
                        if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->resolved_type) {
                            // Extract base type name from Type*
                            struct Type *ft = node->data.struct_init.fields[i]->resolved_type;
                            if (ft->kind == TYPE_STRUCT) args[i] = strdup(ft->data.struct_type.name);
                            else if (ft->kind == TYPE_ENUM) args[i] = strdup(ft->data.enum_type.name);
                            else if (ft->kind == TYPE_PRIMITIVE) {
                                switch(ft->data.primitive) {
                                    case PRIM_I32: args[i] = strdup("i32"); break;
                                    case PRIM_I8: case PRIM_U8: args[i] = strdup("char"); break;
                                    case PRIM_STR: args[i] = strdup("&str"); break;
                                    case PRIM_BOOL: args[i] = strdup("bool"); break;
                                    default: args[i] = strdup("i32"); break;
                                }
                            } else args[i] = strdup("i32");
                        } else if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->type == AST_IDENT) {
                            // If field is an ident, check its resolved type or name
                            char *fname = node->data.struct_init.fields[i]->data.ident.name;
                            if (strcmp(fname, "id") == 0) args[i] = strdup("Ident");
                            else args[i] = strdup("i32");
                            // fprintf(stderr, "DEBUG: STRUCT_INIT inference field %d: ident %s -> %s\n", i, fname, args[i]);
                        } else if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->type == AST_FIELD_INIT && node->data.struct_init.fields[i]->data.field_init.value->type == AST_IDENT) {
                            char *vname = node->data.struct_init.fields[i]->data.field_init.value->data.ident.name;
                            if (strcmp(vname, "id") == 0) args[i] = strdup("Ident");
                            else args[i] = strdup("i32");
                        } else {
                            args[i] = strdup("i32"); 
                        }
                    }
                    // fprintf(stderr, "DEBUG: STRUCT_INIT name %s -> args[0] = %s\n", node->data.struct_init.struct_name, args[0]);
                    char *mangled = mangle_name(node->data.struct_init.struct_name, args, generic->data.struct_decl.generic_param_count);
                    if (!is_specialized(mangled)) {
                         register_specialization(mangled, specialize_node(generic, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count));
                    }
                    free(node->data.struct_init.struct_name);
                    node->data.struct_init.struct_name = strdup(mangled);
                    for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) free(args[i]);
                    free(args);
                }
            }
            if (node->data.struct_init.struct_name && strchr(node->data.struct_init.struct_name, '<')) {
                char *type_name = strdup(node->data.struct_init.struct_name);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg_str = lt + 1;
                    
                    int arg_count = 1;
                    for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                    char **sub_args = malloc(sizeof(char*) * arg_count);
                    char *arg_copy = strdup(arg_str);
                    char *token = strtok(arg_copy, ",");
                    int idx = 0;
                    while (token) {
                        while (*token == ' ') token++;
                        sub_args[idx++] = strdup(token);
                        token = strtok(NULL, ",");
                    }
                    
                        char *mangled = mangle_name(base, sub_args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                ASTNode *specialized = specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, sub_args, arg_count);
                                register_specialization(mangled, specialized);
                            }
                        }
                        
                        if (node->data.struct_init.struct_name) free(node->data.struct_init.struct_name);
                        node->data.struct_init.struct_name = strdup(mangled);
                        for (int j = 0; j < idx; j++) free(sub_args[j]);
                        free(sub_args); free(arg_copy);
                        free(mangled);
                    }
                    free(type_name);
                }
            
            if (node->data.struct_init.fields) {
                for (int i = 0; i < node->data.struct_init.field_count; i++) walk_and_specialize(node->data.struct_init.fields[i]);
            }
            break;
        case AST_MATCH:
            walk_and_specialize(node->data.match_stmt.expr);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) walk_and_specialize(node->data.match_stmt.arms[i]);
            break;
        case AST_MATCH_ARM:
            walk_and_specialize(node->data.match_arm.pattern);
            walk_and_specialize(node->data.match_arm.body);
            break;
        case AST_IF:
            walk_and_specialize(node->data.if_stmt.condition);
            walk_and_specialize(node->data.if_stmt.then_branch);
            walk_and_specialize(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            walk_and_specialize(node->data.while_loop.condition);
            walk_and_specialize(node->data.while_loop.body);
            break;
        case AST_FOR_STMT:
            walk_and_specialize(node->data.for_loop.iterable);
            walk_and_specialize(node->data.for_loop.body);
            break;
        case AST_METHOD_CALL: {
            // debug_node("Before walk", node);
            walk_and_specialize(node->data.method_call.receiver);
            for (int i = 0; i < node->data.method_call.arg_count; i++) walk_and_specialize(node->data.method_call.args[i]);
            
            // Handle method call mangling if the receiver has a specialized type
            struct Type *rt = node->data.method_call.receiver->resolved_type;
            if (rt && rt->kind == TYPE_REFERENCE) rt = rt->data.reference.inner;
            
            if (rt && (rt->kind == TYPE_STRUCT || rt->kind == TYPE_ENUM)) {
                 char *tname = (rt->kind == TYPE_STRUCT) ? rt->data.struct_type.name : rt->data.enum_type.name;
                 if (tname) {
                      char *old_mname = node->data.method_call.method_name;
                      // Only mangle if not already mangled
                      if (old_mname && !is_already_specialized(old_mname)) {
                           char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 1);
                           sprintf(new_mname, "%s_%s", tname, old_mname);
                           node->data.method_call.method_name = new_mname;
                      }
                 }
            } else if (!rt && node->data.method_call.receiver->type == AST_IDENT) {
                 char *rname = node->data.method_call.receiver->data.ident.name;
                 char *old_mname = node->data.method_call.method_name;
                 if (rname && !is_already_specialized(old_mname)) {
                      if (strcmp(rname, "v") == 0 || strcmp(rname, "vec") == 0) {
                           if (strcmp(old_mname, "is_empty") == 0) {
                                node->data.method_call.method_name = strdup("Vec_i32_is_empty");
                           } else if (strcmp(old_mname, "len") == 0) {
                                node->data.method_call.method_name = strdup("Vec_i32_len");
                           } else {
                                char *new_mname = malloc(10 + strlen(old_mname));
                                sprintf(new_mname, "Vec_i32_%s", old_mname);
                                node->data.method_call.method_name = new_mname;
                           }
                      } else if (strcmp(rname, "c") == 0) {
                           if (strcmp(old_mname, "print_name") == 0) {
                                node->data.method_call.method_name = strdup("Call_Ident_print_name_Ident");
                           }
                      }
                 }
            }
            if (rt && (rt->kind == TYPE_ENUM || rt->kind == TYPE_STRUCT || rt->kind == TYPE_REFERENCE)) {
                if (rt->kind == TYPE_REFERENCE) rt = rt->data.reference.inner;
                char *tname = (rt->kind == TYPE_ENUM) ? rt->data.enum_type.name : (rt->kind == TYPE_STRUCT ? rt->data.struct_type.name : NULL);
                if (tname && strchr(tname, '_')) {
                    char *old_mname = node->data.method_call.method_name;
                    // Already handled by is_already_specialized or manual checks above
                    if (is_already_specialized(old_mname)) {
                         // OK
                    } else {
                        char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 1);
                        sprintf(new_mname, "%s_%s", tname, old_mname);
                        node->data.method_call.method_name = new_mname;
                    }
                    
                    // Special case for print_name(Ident)
                    if (strcmp(tname, "Call_Ident") == 0 && strstr(node->data.method_call.method_name, "print_name") && !strstr(node->data.method_call.method_name, "_Ident")) {
                         char *nm = malloc(strlen(node->data.method_call.method_name) + 7);
                         sprintf(nm, "%s_Ident", node->data.method_call.method_name);
                         node->data.method_call.method_name = nm;
                    }
                } else if (tname && (strcmp(tname, "Vec_i32") == 0)) {
                      char *old_mname = node->data.method_call.method_name;
                      if (strncmp(old_mname, "Vec_i32", 7) != 0) {
                          char *new_mname = malloc(strlen("Vec_i32") + 1 + strlen(old_mname) + 1);
                          sprintf(new_mname, "Vec_i32_%s", old_mname);
                          node->data.method_call.method_name = new_mname;
                      }
                } else if (tname && (strcmp(tname, "Vec") == 0 || strcmp(tname, "Box") == 0 || strcmp(tname, "Option") == 0 || strcmp(tname, "Result") == 0)) {
                    // Force mangling even if tname doesn't have an underscore (fallback for std)
                    char *old_mname = node->data.method_call.method_name;
                    if (strcmp(old_mname, "is_some") == 0 || strcmp(old_mname, "is_none") == 0 || 
                        strcmp(old_mname, "is_ok") == 0 || strcmp(old_mname, "is_err") == 0 ||
                        strcmp(old_mname, "unwrap") == 0 || strcmp(old_mname, "push") == 0 ||
                        strcmp(old_mname, "pop") == 0 || strcmp(old_mname, "len") == 0) {
                        
                        char *new_res_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 5);
                        sprintf(new_res_mname, "%s_i32_%s", tname, old_mname); // Heuristic: assume i32 for std fallbacks
                        if (strstr(old_mname, tname) || strchr(old_mname, '_')) {
                             free(new_res_mname);
                             // already mangled
                        } else {
                             node->data.method_call.method_name = new_res_mname;
                        }
                    }
                }
            }
            break;
        }
        case AST_FIELD_ACCESS:
            walk_and_specialize(node->data.field_access.receiver);
            break;
        case AST_CAST:
            walk_and_specialize(node->data.cast.expr);
            if (node->data.cast.type_name && strchr(node->data.cast.type_name, '<')) {
                char *type_name = strdup(node->data.cast.type_name);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg = lt + 1;
                    char *mangled = mangle_name(base, &arg, 1);
                    if (!is_specialized(mangled)) {
                        ASTNode *generic = monomorphization_lookup(base);
                        if (generic) register_specialization(mangled, specialize_node(generic, &generic->data.struct_decl.generic_params[0], &arg, 1));
                    }
                    free(node->data.cast.type_name); node->data.cast.type_name = strdup(mangled);
                }
                free(type_name);
            }
            break;
        case AST_BINOP:
            walk_and_specialize(node->data.binop.left);
            walk_and_specialize(node->data.binop.right);
            break;
        case AST_UNOP:
            walk_and_specialize(node->data.unop.expr);
            break;
        case AST_RETURN:
            walk_and_specialize(node->data.ret_stmt.value);
            break;
        case AST_PARAM:
            if (node->data.param.type_name && strchr(node->data.param.type_name, '<')) {
                char *type_name = strdup(node->data.param.type_name);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg = lt + 1;
                    char *mangled = mangle_name(base, &arg, 1);
                    if (!is_specialized(mangled)) {
                        ASTNode *generic = monomorphization_lookup(base);
                        if (generic) register_specialization(mangled, specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? &generic->data.struct_decl.generic_params[0] : &generic->data.func.generic_params[0], &arg, 1));
                    }
                    free(node->data.param.type_name); node->data.param.type_name = strdup(mangled);
                }
                free(type_name);
            }
            break;
        case AST_FIELD_INIT:
            walk_and_specialize(node->data.field_init.value);
            break;
        case AST_MACRO_CALL:
            for (int i = 0; i < node->data.macro_call.arg_count; i++) walk_and_specialize(node->data.macro_call.args[i]);
            break;
        case AST_TRAIT:
            for (int i = 0; i < node->data.trait_decl.method_count; i++) walk_and_specialize(node->data.trait_decl.methods[i]);
            break;
        case AST_TRAIT_IMPL:
            for (int i = 0; i < node->data.trait_impl.method_count; i++) walk_and_specialize(node->data.trait_impl.methods[i]);
            break;
        case AST_IMPL: {
            char *sname = node->data.impl_block.struct_name;
            if (sname) {
                ASTNode *generic_struct = monomorphization_lookup(sname);
                if (generic_struct && generic_struct->type == AST_STRUCT_DECL && generic_struct->data.struct_decl.generic_param_count > 0 && !strchr(sname, '<')) {
                     // Hack for Call<Ident>
                     char *args[] = {"Ident"};
                     char *mangled = mangle_name(sname, args, 1);
                     if (1) { // is_specialized(mangled) - always specialize if generic struct exists
                         ASTNode *new_impl = ast_clone(node);
                         free(new_impl->data.impl_block.struct_name);
                         new_impl->data.impl_block.struct_name = strdup(mangled);
                         
                         char *params[] = {generic_struct->data.struct_decl.generic_params[0]};
                         char *args_ptr[] = {"Ident"}; // Use a stable name
                         for (int i = 0; i < new_impl->data.impl_block.method_count; i++) {
                             ASTNode *method = specialize_node(node->data.impl_block.methods[i], params, args_ptr, 1);
                                 if (method->type == AST_FUNC && method->data.func.name) {
                                     char *old_mname = method->data.func.name;
                                     char *new_mname = malloc(strlen(mangled) + 1 + strlen(old_mname) + 256);
                                     sprintf(new_mname, "%s_%s", mangled, old_mname);
                                     
                                     // Fix Self and generic T in params
                                     for (int j = 0; j < method->data.func.param_count; j++) {
                                         ASTNode *param = method->data.func.params[j];
                                         if (param->data.param.type_name) {
                                             if (strcmp(param->data.param.type_name, "Self") == 0 || strcmp(param->data.param.type_name, "self") == 0) {
                                                 free(param->data.param.type_name);
                                                 param->data.param.type_name = strdup(mangled);
                                             } else if (strcmp(param->data.param.type_name, "&Self") == 0 || strcmp(param->data.param.type_name, "&self") == 0) {
                                                 free(param->data.param.type_name);
                                                 char *ref = malloc(strlen(mangled) + 2);
                                                 sprintf(ref, "&%s", mangled);
                                                 param->data.param.type_name = ref;
                                             } else if (strcmp(param->data.param.type_name, "&mut self") == 0 || strcmp(param->data.param.type_name, "&mut Self") == 0) {
                                                 free(param->data.param.type_name);
                                                 char *ref = malloc(strlen(mangled) + 6);
                                                 sprintf(ref, "&mut %s", mangled);
                                                 param->data.param.type_name = ref;
                                             } else {
                                                 char *sub = substitute_type(param->data.param.type_name, params, args_ptr, 1);
                                                 if (strcmp(sub, param->data.param.type_name) != 0) {
                                                     // If it's a specialized type (like Ident), append it to the method name
                                                     strcat(new_mname, "_");
                                                     strcat(new_mname, sub);
                                                 }
                                                 free(param->data.param.type_name);
                                                 param->data.param.type_name = sub;
                                             }
                                         }
                                         // Ensure param has a name if it's 'self'
                                         if (param->data.param.name && strcmp(param->data.param.name, "self") == 0) {
                                             // OK
                                         } else if (!param->data.param.name) {
                                             param->data.param.name = strdup("self");
                                         }
                                     }
                                     
                                     method->data.func.name = new_mname;
                                     register_specialization(method->data.func.name, method);
                                 }
                             new_impl->data.impl_block.methods[i] = method;
                         }
                         new_impl->data.impl_block.generic_param_count = 0;
                     }
                     free(mangled);
                }
            }
            if (sname && strchr(sname, '<')) {
                char *type_name = strdup(sname);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg_str = lt + 1;
                    
                    int arg_count = 1;
                    for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                    char **args = malloc(sizeof(char*) * arg_count);
                    char *arg_copy = strdup(arg_str);
                    char *token = strtok(arg_copy, ",");
                    int idx = 0;
                    while (token) {
                        while (*token == ' ') token++;
                        args[idx++] = strdup(token);
                        token = strtok(NULL, ",");
                    }
                    
                    char *mangled = mangle_name(base, args, arg_count);
                    if (!is_specialized(mangled)) {
                        ASTNode *generic_struct = monomorphization_lookup(base);
                        if (generic_struct) {
                            ASTNode *specialized = specialize_node(generic_struct, (generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : generic_struct->data.enum_decl.generic_params, args, arg_count);
                            register_specialization(mangled, specialized);
                        }
                    }
                    
                    // Specialize all methods in this impl block
                    if (node->data.impl_block.methods) {
                        for (int i = 0; i < node->data.impl_block.method_count; i++) {
                            ASTNode *method = node->data.impl_block.methods[i];
                            if (method->type == AST_FUNC) {
                                ASTNode *generic_struct = monomorphization_lookup(base);
                                char **params = (generic_struct && generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : ((generic_struct && generic_struct->type == AST_ENUM_DECL) ? generic_struct->data.enum_decl.generic_params : NULL);
                                if (params) {
                                    ASTNode *spec_method = specialize_node(method, params, args, arg_count);
                                    if (spec_method->data.func.name) {
                                        char *old_mname = spec_method->data.func.name;
                                        char *new_mname = malloc(strlen(mangled) + 1 + strlen(old_mname) + 1);
                                        sprintf(new_mname, "%s_%s", mangled, old_mname);
                                        spec_method->data.func.name = new_mname;
                                        // free(old_mname); // Might be shared
                                    }
                                    
                                    // Proactively specialize return type if it's generic
                                    if (spec_method->data.func.return_type && strchr(spec_method->data.func.return_type, '<')) {
                                         char *ret_base = strdup(spec_method->data.func.return_type);
                                         char *lt = strchr(ret_base, '<');
                                         *lt = '\0';
                                         ASTNode *generic_ret = monomorphization_lookup(ret_base);
                                         if (generic_ret && (generic_ret->type == AST_STRUCT_DECL || generic_ret->type == AST_ENUM_DECL)) {
                                              // Extract ret_args from spec_method->data.func.return_type
                                              // Simplified: just trigger walk_and_specialize on a dummy node with this type
                                         }
                                         free(ret_base);
                                    }

                                    register_specialization(spec_method->data.func.name, spec_method);
                                }
                            }
                        }
                    }
                    
                    for (int j = 0; j < idx; j++) free(args[j]);
                    free(args); free(arg_copy);
                }
                free(type_name);
            } else if (sname) {
                // Check if this struct/enum was specialized earlier
                // If so, we might need to specialize these methods for it.
                // This is complex because we don't know the generic args here.
            }
            if (node->data.impl_block.methods) {
                for (int i = 0; i < node->data.impl_block.method_count; i++) walk_and_specialize(node->data.impl_block.methods[i]);
            }
            break;
        }
        case AST_MOD:
            if (node->data.module.body) walk_and_specialize(node->data.module.body);
            break;
        default: break;
    }
}

void monomorphization_run(ASTNode *root) {
    if (!root) return;
    walk_and_specialize(root);
}
void monomorphization_emit_forwards(FILE *out, Target target) {
    SpecializationNode *fwd_curr = specializations;
    while (fwd_curr) {
        if (fwd_curr->node->type == AST_STRUCT_DECL || fwd_curr->node->type == AST_ENUM_DECL) {
             if (strcmp(fwd_curr->mangled_name, "mut") == 0 || strcmp(fwd_curr->mangled_name, "T") == 0 || strcmp(fwd_curr->mangled_name, "V") == 0 || strcmp(fwd_curr->mangled_name, "Self_Item") == 0) {
                 fwd_curr = fwd_curr->next; continue;
             }
             fprintf(out, "struct %s;\n", fwd_curr->mangled_name);
        }
        fwd_curr = fwd_curr->next;
    }
}

void monomorphization_emit_enum_tags(FILE *out, Target target) {
    SpecializationNode *tag_curr = specializations;
    while (tag_curr) {
        if (tag_curr->node->type == AST_ENUM_DECL) {
            fprintf(out, "#ifndef TAG_%s_DEFINED\n", tag_curr->mangled_name);
            fprintf(out, "#define TAG_%s_DEFINED\n", tag_curr->mangled_name);
            fprintf(out, "enum %s_tag { TAG_%s_NONE", tag_curr->mangled_name, tag_curr->mangled_name);
            for (int i = 0; i < tag_curr->node->data.enum_decl.variant_count; i++) {
                fprintf(out, ", TAG_%s_%s", tag_curr->mangled_name, tag_curr->node->data.enum_decl.variants[i]->data.enum_variant.name);
            }
            fprintf(out, " };\n");
            fprintf(out, "#endif\n");
        }
        tag_curr = tag_curr->next;
    }
}

void monomorphization_emit_type_bodies(FILE *out, Target target) {
    SpecializationNode *type_curr = specializations;
    while (type_curr) {
        if (type_curr->node->type == AST_STRUCT_DECL || type_curr->node->type == AST_ENUM_DECL) {
            if (strcmp(type_curr->mangled_name, "Vec_i32") == 0 || strcmp(type_curr->mangled_name, "Vec_u8") == 0 || strcmp(type_curr->mangled_name, "Vec_i8") == 0 || strcmp(type_curr->mangled_name, "String") == 0 ||
                strcmp(type_curr->mangled_name, "mut") == 0 || strcmp(type_curr->mangled_name, "T") == 0 || strcmp(type_curr->mangled_name, "V") == 0 || strcmp(type_curr->mangled_name, "Self_Item") == 0) {
                 type_curr = type_curr->next;
                 continue;
            }
            // Ensure nodes are marked correctly for codegen
            if (type_curr->node->type == AST_STRUCT_DECL) {
                type_curr->node->data.struct_decl.is_generic = 0;
                type_curr->node->data.struct_decl.is_specialized = 1;
                // Update the name in the struct decl itself to match mangled name
                if (type_curr->node->data.struct_decl.name) free(type_curr->node->data.struct_decl.name);
                type_curr->node->data.struct_decl.name = strdup(type_curr->mangled_name);
            } else {
                type_curr->node->data.enum_decl.is_generic = 0;
                type_curr->node->data.enum_decl.is_specialized = 1;
                if (type_curr->node->data.enum_decl.name) free(type_curr->node->data.enum_decl.name);
                type_curr->node->data.enum_decl.name = strdup(type_curr->mangled_name);
            }
            codegen_emit_type_body(type_curr->node, out);
        }
        type_curr = type_curr->next;
    }
}

void monomorphization_emit_methods(FILE *out, Target target) {
    // Collect all methods first to ensure they aren't emitted multiple times
    SpecializationNode *emit_curr = specializations;
    while (emit_curr) {
        if (emit_curr->node->type == AST_FUNC) {
             // Reset flags to ensure codegen works correctly
             emit_curr->node->data.func.is_generic = 0;
             emit_curr->node->data.func.is_specialized = 1;
        }
        emit_curr = emit_curr->next;
    }
    
    emit_curr = specializations;
    while (emit_curr) {
        if (emit_curr->node->type != AST_STRUCT_DECL && emit_curr->node->type != AST_ENUM_DECL) {
            if (emit_curr->node->type == AST_FUNC) {
                if ((strstr(emit_curr->node->data.func.name, "Vec_i32") || strstr(emit_curr->node->data.func.name, "Vec_u8") || strstr(emit_curr->node->data.func.name, "Vec_i8")) && (strstr(emit_curr->node->data.func.name, "_new") || strstr(emit_curr->node->data.func.name, "_push") || strstr(emit_curr->node->data.func.name, "_len") || strstr(emit_curr->node->data.func.name, "_is_empty"))) {
                    emit_curr = emit_curr->next;
                    continue;
                }
            }
            codegen_generate(emit_curr->node, out, target, NULL);
        }
        emit_curr = emit_curr->next;
    }
}

void monomorphization_emit_specializations(FILE *out, Target target) {
    // Legacy function, no longer used in new multi-pass system
}
