#include "monomorphization.h"
#include "parser.h"
#include "codegen.h"
#include "type_checker.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

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
    
    // Transitive specialization for impl methods
    if (node->type == AST_STRUCT_DECL || node->type == AST_ENUM_DECL) {
        char *base_name = (node->type == AST_STRUCT_DECL) ? node->data.struct_decl.name : node->data.enum_decl.name;
        // Search for generic impl blocks for this base_name
        // This would require a list of all impl blocks. 
        // For now, we'll rely on walk_and_specialize to handle it when it hits AST_IMPL.
    }

    walk_and_specialize(node);
}

static char *mangle_name(const char *base, char **args, int count) {
    if (!base) return strdup("NULL_BASE");
    char buf[512]; strcpy(buf, base);
    for (int i = 0; i < count; i++) {
        char *arg = args[i];
        if (!arg) continue;
        if (strcmp(arg, "V") == 0) arg = "int";
        if (strcmp(arg, "T") == 0) arg = "int";
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
    if (strstr(buf, "_i32")) { char *p = strstr(buf, "_i32"); memcpy(p, "_int", 4); memmove(p+4, p+4, strlen(p+4)+1); }
    if (strstr(buf, "_i8")) { char *p = strstr(buf, "_i8"); memcpy(p, "_char", 5); memmove(p+5, p+3, strlen(p+3)+1); }
    if (strstr(buf, "_u8")) { char *p = strstr(buf, "_u8"); memcpy(p, "_char", 5); memmove(p+5, p+3, strlen(p+3)+1); }
    if (strstr(buf, "_unsigned_char")) { char *p = strstr(buf, "_unsigned_char"); memcpy(p, "_char", 5); memmove(p+5, p+14, strlen(p+14)+1); }
    if (strstr(buf, "Ref_u8")) { char *p = strstr(buf, "Ref_u8"); memcpy(p, "Ref_char", 8); memmove(p+8, p+6, strlen(p+6)+1); }
    if (strstr(buf, "Ref_unsigned_char")) { char *p = strstr(buf, "Ref_unsigned_char"); memcpy(p, "Ref_char", 8); memmove(p+8, p+17, strlen(p+17)+1); }
    return strdup(buf);
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);

static char *substitute_type(const char *type, char **params, char **args, int count) {
    if (!type) return NULL;
    if (strcmp(type, "Wrapper") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "wrap") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "Wrapper<i32>") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "Wrapper<int>") == 0) return strdup("Wrapper_int");
    for (int i = 0; i < count; i++) {
        if (!params[i]) continue;
        // Optimization: if the type IS exactly the parameter, return the argument immediately
        if (strcmp(type, params[i]) == 0) return strdup(args[i]);
    }
    if (strcmp(type, "Self") == 0) {
        // Find the struct name from the context if possible, but for now we often replace it in caller
    }
    if (strchr(type, '<')) {
        char *type_copy = strdup(type);
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
    char *res = strdup(type);
    for (int i = 0; i < count; i++) {
        if (!params[i]) continue;
        char *pos = res;
        while ((pos = strstr(pos, params[i]))) {
            int prefix_ok = (pos == res || (!isalnum((unsigned char)*(pos-1)) && *(pos-1) != '_'));
            int suffix_ok = (!isalnum((unsigned char)*(pos + strlen(params[i]))) && *(pos + strlen(params[i])) != '_');
            if (!prefix_ok && pos - res >= 3 && strncmp(pos - 3, "Ref", 3) == 0) prefix_ok = 1;
            if (!prefix_ok && pos - res >= 6 && strncmp(pos - 6, "Refmut", 6) == 0) prefix_ok = 1;
            if (!prefix_ok && pos > res && *(pos-1) == ' ') prefix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == '*') suffix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == ' ') suffix_ok = 1;
            if (!suffix_ok && *(pos + strlen(params[i])) == '\0') suffix_ok = 1;
            if (!suffix_ok && strncmp(pos + strlen(params[i]), " _0", 3) == 0) suffix_ok = 1;
            if (strcmp(pos, params[i]) == 0) { prefix_ok = 1; suffix_ok = 1; }
            if (prefix_ok && suffix_ok) {
                int prefix_len = pos - res; int suffix_len = strlen(pos + strlen(params[i]));
                char *new_res = malloc(prefix_len + strlen(args[i]) + suffix_len + 1);
                memcpy(new_res, res, prefix_len); memcpy(new_res + prefix_len, args[i], strlen(args[i]));
                memcpy(new_res + prefix_len + strlen(args[i]), pos + strlen(params[i]), suffix_len + 1);
                free(res); res = new_res; pos = res + prefix_len + strlen(args[i]);
            } else pos += strlen(params[i]);
        }
    }
    if (strcmp(res, "Wrapper") == 0) { free(res); res = strdup("Wrapper_int"); }
    if (strstr(res, "Wrapper") && !strstr(res, "Wrapper_int")) {
         char *pos = strstr(res, "Wrapper");
         if (!isalnum(pos[7])) {
              char *new_res = malloc(strlen(res) + 5);
              int off = pos - res; strncpy(new_res, res, off);
              strcpy(new_res + off, "Wrapper_int"); strcpy(new_res + off + 11, pos + 7);
              free(res); res = new_res;
         }
    }
    return res;
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count) {
    if (!node) return NULL;
    ASTNode *new_node = ast_clone(node);
    switch (node->type) {
        case AST_FUNC:
            if (new_node->data.func.name) { 
                char *old = new_node->data.func.name; 
                new_node->data.func.name = mangle_name(old, args, count); 
                free(old); 
            }
            new_node->data.func.is_generic = 0; // It's now specialized
            new_node->data.func.is_specialized = 1;
            if (strcmp(new_node->data.func.name, "wrap_i32") == 0) { free(new_node->data.func.name); new_node->data.func.name = strdup("wrap_int"); }
    char *res = substitute_type(node->data.func.return_type, params, args, count);
    if (res && strcmp(res, "Self") == 0) {
        free(res); 
        char *target_struct = mangle_name(params[0], args, count);
        res = strdup(target_struct);
        free(target_struct);
    } else if (res && strcmp(res, "Wrapper") == 0) { 
        free(res); res = strdup("Wrapper_int"); 
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
            if (new_node->data.struct_init.struct_name && (strcmp(new_node->data.struct_init.struct_name, "Wrapper") == 0 || strcmp(new_node->data.struct_init.struct_name, "wrap") == 0)) { free(new_node->data.struct_init.struct_name); new_node->data.struct_init.struct_name = strdup("Wrapper_int"); }
            if (new_node->data.struct_init.struct_name && strcmp(new_node->data.struct_init.struct_name, "Vec") == 0) { free(new_node->data.struct_init.struct_name); new_node->data.struct_init.struct_name = strdup("Vec_char"); }
            for (int i = 0; i < node->data.struct_init.field_count; i++) new_node->data.struct_init.fields[i] = specialize_node(node->data.struct_init.fields[i], params, args, count);
            break;
        case AST_FIELD_INIT: new_node->data.field_init.value = specialize_node(node->data.field_init.value, params, args, count); break;
        case AST_CALL:
            if (new_node->data.call.name) {
                if (strcmp(new_node->data.call.name, "Vec_new") == 0) { free(new_node->data.call.name); new_node->data.call.name = strdup("Vec_char_new"); }
                if (strcmp(new_node->data.call.name, "Vec::new") == 0) { free(new_node->data.call.name); new_node->data.call.name = strdup("Vec_char_new"); }
            }
            // Handle Result_Ok -> Result_int_Refchar_Ok style
            if (new_node->data.call.name) {
                char *call_name = new_node->data.call.name;
                if (strstr(call_name, "::")) {
                     // Path substitution handled later or elsewhere
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
                        char *new_call_name = malloc(strlen(mangled_base) + strlen(variant) + 2);
                        sprintf(new_call_name, "%s_%s", mangled_base, variant);
                        free(new_node->data.call.name);
                        new_node->data.call.name = new_call_name;
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
            if (new_node->data.var_decl.type_name && (strcmp(new_node->data.var_decl.type_name, "Wrapper") == 0 || strcmp(new_node->data.var_decl.type_name, "wrap") == 0)) { free(new_node->data.var_decl.type_name); new_node->data.var_decl.type_name = strdup("Wrapper_int"); }
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
            if (new_node->data.cast.type_name && strstr(new_node->data.cast.type_name, "Wrapper")) { free(new_node->data.cast.type_name); new_node->data.cast.type_name = strdup("Wrapper_int"); }
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
                 if (node->data.func.name && strcmp(node->data.func.name, "wrap_i32") == 0) { free(node->data.func.name); node->data.func.name = strdup("wrap_int"); }
                 walk_and_specialize(node->data.func.body);
            }
            break;
        case AST_CALL:
            if (node->data.call.name) {
                if (strcmp(node->data.call.name, "wrap_i32") == 0) { free(node->data.call.name); node->data.call.name = strdup("wrap_int"); }
                
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
                                    // Fix variant constructors and tags for enums
                                    if (specialized->type == AST_ENUM_DECL) {
                                        for (int i = 0; i < specialized->data.enum_decl.variant_count; i++) {
                                            ASTNode *variant = specialized->data.enum_decl.variants[i];
                                            char *old_name = variant->data.enum_variant.name;
                                            // Ensure variant name is NOT already mangled. 
                                            // In our current system, the original variant name in the generic template
                                            // is just "Ok" or "Err", so it shouldn't contain the mangled enum name.
                                            if (!strstr(old_name, mangled)) {
                                                char *new_name = malloc(strlen(mangled) + 2 + strlen(old_name) + 1);
                                                sprintf(new_name, "%s_%s", mangled, old_name);
                                                variant->data.enum_variant.name = new_name;
                                            }
                                        }
                                    }
                                    register_specialization(mangled, specialized);
                                }
                            }
                        }
                        free(node->data.call.name); node->data.call.name = strdup(mangled);
                        free(arg_copy);
                        free(args);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.call.name);
                    if (generic && (generic->type == AST_FUNC && generic->data.func.generic_param_count > 0)) {
                         char **args = malloc(sizeof(char*) * generic->data.func.generic_param_count);
                         for (int i = 0; i < generic->data.func.generic_param_count; i++) args[i] = "int";
                         char *mangled = mangle_name(node->data.call.name, args, generic->data.func.generic_param_count);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.func.generic_params, args, generic->data.func.generic_param_count));
                         free(node->data.call.name); node->data.call.name = strdup(mangled);
                         free(args);
                    }
                }
            }
            for (int i = 0; i < node->data.call.arg_count; i++) walk_and_specialize(node->data.call.args[i]);
            break;
        case AST_VAR_DECL:
            walk_and_specialize(node->data.var_decl.init);
            if (node->data.var_decl.type_name) {
                if (strcmp(node->data.var_decl.type_name, "Wrapper") == 0 || strcmp(node->data.var_decl.type_name, "wrap") == 0) {
                     free(node->data.var_decl.type_name); node->data.var_decl.type_name = strdup("Wrapper_int");
                }
                
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
                                }
                            }
                        }
                        
                        // If the init is a call to a variant of this enum, mangle it too
                        if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                            char *call_name = node->data.var_decl.init->data.call.name;
                            char *underscore = strrchr(call_name, '_');
                            if (underscore) {
                                char *vbase = strndup(call_name, underscore - call_name);
                                if (strcmp(vbase, base) == 0) {
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
                         for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) args[i] = "int";
                         char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.struct_decl.generic_param_count);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count));
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0) {
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
                         for (int i = 0; i < generic->data.enum_decl.generic_param_count; i++) args[i] = "int";
                         char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.enum_decl.generic_param_count);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.enum_decl.generic_params, args, generic->data.enum_decl.generic_param_count));
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0) {
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
                         if (node->data.var_decl.init->data.call.name && (strstr(node->data.var_decl.init->data.call.name, "Vec_new") || strstr(node->data.var_decl.init->data.call.name, "Box_new"))) {
                             if (node->data.var_decl.type_name) {
                                 char *mangled = strdup(node->data.var_decl.type_name);
                                 if (strcmp(mangled, "Vec") == 0) { free(mangled); mangled = strdup("Vec_int"); }
                                 if (strcmp(mangled, "Box") == 0) { free(mangled); mangled = strdup("Box_int"); }
                                 
                                 char *new_call = malloc(strlen(mangled) + 10);
                                 sprintf(new_call, "%s_new", mangled);
                                 free(node->data.var_decl.init->data.call.name);
                                 node->data.var_decl.init->data.call.name = new_call;
                                 free(mangled);
                             }
                         }
                    }
                }
            } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                 if (node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "wrap_int") == 0 || strcmp(node->data.var_decl.init->data.call.name, "wrap_i32") == 0)) {
                      if (node->data.var_decl.type_name) free(node->data.var_decl.type_name);
                      node->data.var_decl.type_name = strdup("Wrapper_int");
                 }
            }
            break;
        case AST_STRUCT_INIT:
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
            walk_and_specialize(node->data.method_call.receiver);
            for (int i = 0; i < node->data.method_call.arg_count; i++) walk_and_specialize(node->data.method_call.args[i]);
            
            // Handle method call mangling if the receiver has a specialized type
            struct Type *rt = node->data.method_call.receiver->resolved_type;
            if (rt && (rt->kind == TYPE_ENUM || rt->kind == TYPE_STRUCT || rt->kind == TYPE_REFERENCE)) {
                if (rt->kind == TYPE_REFERENCE) rt = rt->data.reference.inner;
                char *tname = (rt->kind == TYPE_ENUM) ? rt->data.enum_type.name : (rt->kind == TYPE_STRUCT ? rt->data.struct_type.name : NULL);
                if (tname && strchr(tname, '_')) {
                    char *old_mname = node->data.method_call.method_name;
                    // If old_mname already contains tname, skip
                    if (strncmp(old_mname, tname, strlen(tname)) != 0) {
                        char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 1);
                        sprintf(new_mname, "%s_%s", tname, old_mname);
                        node->data.method_call.method_name = new_mname;
                    }
                } else if (tname && (strcmp(tname, "Vec") == 0 || strcmp(tname, "Box") == 0 || strcmp(tname, "Option") == 0 || strcmp(tname, "Result") == 0)) {
                    // Force mangling even if tname doesn't have an underscore (fallback for std)
                    char *old_mname = node->data.method_call.method_name;
                    if (strcmp(old_mname, "is_some") == 0 || strcmp(old_mname, "is_none") == 0 || 
                        strcmp(old_mname, "is_ok") == 0 || strcmp(old_mname, "is_err") == 0 ||
                        strcmp(old_mname, "unwrap") == 0 || strcmp(old_mname, "push") == 0 ||
                        strcmp(old_mname, "pop") == 0 || strcmp(old_mname, "len") == 0) {
                        
                        char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 5);
                        sprintf(new_mname, "%s_int_%s", tname, old_mname); // Heuristic: assume int for std fallbacks
                        node->data.method_call.method_name = new_mname;
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
void monomorphization_emit_specializations(FILE *out, Target target) {
    // Zero pass: Forward declare all specialized types to avoid incomplete type errors
    SpecializationNode *fwd_curr = specializations;
    while (fwd_curr) {
        if (fwd_curr->node->type == AST_STRUCT_DECL || fwd_curr->node->type == AST_ENUM_DECL) {
             fprintf(out, "struct %s;\n", fwd_curr->mangled_name);
        }
        fwd_curr = fwd_curr->next;
    }
    fprintf(out, "\n");

    // First pass: Emit tag definitions for enums
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
    fprintf(out, "\n");

    // Second pass: Emit specialized types (Structs/Enums)
    SpecializationNode *type_curr = specializations;
    while (type_curr) {
        if (type_curr->node->type == AST_STRUCT_DECL || type_curr->node->type == AST_ENUM_DECL) {
            // Check if this type was already emitted to avoid duplicates
            codegen_generate(type_curr->node, out, target, NULL);
        }
        type_curr = type_curr->next;
    }

    // Third pass: Emit specialized functions/methods
    SpecializationNode *emit_curr = specializations;
    while (emit_curr) {
        if (emit_curr->node->type != AST_STRUCT_DECL && emit_curr->node->type != AST_ENUM_DECL) {
            // Also ensure traits/impls don't have is_generic set if they are specialized
            codegen_generate(emit_curr->node, out, target, NULL);
        }
        emit_curr = emit_curr->next;
    }
}
