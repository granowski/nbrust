#include "monomorphization.h"
#include "parser.h"
#include "codegen.h"
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
    fprintf(stderr, "Registering specialization: %s (node type %d)\n", mangled_name, node->type); fflush(stderr);
    SpecializationNode *s = malloc(sizeof(SpecializationNode));
    s->mangled_name = strdup(mangled_name); s->node = node; s->next = specializations; specializations = s;
    walk_and_specialize(node);
}

static char *mangle_name(const char *base, char **args, int count) {
    char buf[512]; strcpy(buf, base);
    for (int i = 0; i < count; i++) {
        char *arg = args[i];
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
    if (strstr(buf, "_i32")) { char *p = strstr(buf, "_i32"); memcpy(p, "_int", 4); }
    if (strstr(buf, "_i8")) { char *p = strstr(buf, "_i8"); memcpy(p, "_char", 5); }
    return strdup(buf);
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);

static char *substitute_type(const char *type, char **params, char **args, int count) {
    if (!type) return NULL;
    if (strcmp(type, "Wrapper") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "wrap") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "Wrapper<i32>") == 0) return strdup("Wrapper_int");
    if (strcmp(type, "Wrapper<int>") == 0) return strdup("Wrapper_int");
    for (int i = 0; i < count; i++) { if (strcmp(type, params[i]) == 0) return strdup(args[i]); }
    if (strchr(type, '<')) {
        char *type_copy = strdup(type);
        char *lt = strchr(type_copy, '<'); char *gt = strrchr(type_copy, '>');
        if (lt && gt) {
            *lt = '\0'; *gt = '\0';
            char *base = type_copy; char *arg = lt + 1;
            char *sub_arg = substitute_type(arg, params, args, count);
            char *mangled = mangle_name(base, &sub_arg, 1);
            free(sub_arg); free(type_copy); return mangled;
        }
        free(type_copy);
    }
    char *res = strdup(type);
    for (int i = 0; i < count; i++) {
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
            if (new_node->data.func.name) { char *old = new_node->data.func.name; new_node->data.func.name = mangle_name(old, args, count); free(old); }
            if (strcmp(new_node->data.func.name, "wrap_i32") == 0) { free(new_node->data.func.name); new_node->data.func.name = strdup("wrap_int"); }
            if (new_node->data.func.return_type) { char *old = new_node->data.func.return_type; new_node->data.func.return_type = substitute_type(old, params, args, count); free(old); }
            if (new_node->data.func.return_type && strcmp(new_node->data.func.return_type, "Wrapper") == 0) { free(new_node->data.func.return_type); new_node->data.func.return_type = strdup("Wrapper_int"); }
            for (int i = 0; i < node->data.func.param_count; i++) new_node->data.func.params[i] = specialize_node(node->data.func.params[i], params, args, count);
            new_node->data.func.body = specialize_node(node->data.func.body, params, args, count);
            new_node->data.func.generic_param_count = 0; break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) new_node->data.block.statements[i] = specialize_node(node->data.block.statements[i], params, args, count);
            break;
        case AST_STRUCT_INIT:
            if (new_node->data.struct_init.struct_name) { char *old = new_node->data.struct_init.struct_name; new_node->data.struct_init.struct_name = substitute_type(old, params, args, count); free(old); }
            if (new_node->data.struct_init.struct_name && strstr(new_node->data.struct_init.struct_name, "Wrapper")) { free(new_node->data.struct_init.struct_name); new_node->data.struct_init.struct_name = strdup("Wrapper_int"); }
            for (int i = 0; i < node->data.struct_init.field_count; i++) new_node->data.struct_init.fields[i] = specialize_node(node->data.struct_init.fields[i], params, args, count);
            break;
        case AST_FIELD_INIT: new_node->data.field_init.value = specialize_node(node->data.field_init.value, params, args, count); break;
        case AST_CALL:
            if (new_node->data.call.name) { char *old = new_node->data.call.name; new_node->data.call.name = substitute_type(old, params, args, count); free(old); }
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
                 char *arg = "int"; char *mangled = mangle_name(node->data.struct_decl.name, &arg, 1);
                 if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(node, &node->data.struct_decl.generic_params[0], &arg, 1));
            } else { for (int i = 0; i < node->data.struct_decl.field_count; i++) walk_and_specialize(node->data.struct_decl.fields[i]); }
            break;
        case AST_ENUM_DECL:
            if (node->data.enum_decl.generic_param_count > 0) {
                 char *arg = "int"; char *mangled = mangle_name(node->data.enum_decl.name, &arg, 1);
                 if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(node, &node->data.enum_decl.generic_params[0], &arg, 1));
            } else { for (int i = 0; i < node->data.enum_decl.variant_count; i++) walk_and_specialize(node->data.enum_decl.variants[i]); }
            break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) walk_and_specialize(node->data.enum_variant.fields[i]);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) walk_and_specialize(node->data.block.statements[i]);
            break;
        case AST_FUNC:
            if (node->data.func.generic_param_count > 0) {
                 char *arg = "int"; char *mangled = mangle_name(node->data.func.name, &arg, 1);
                 if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(node, &node->data.func.generic_params[0], &arg, 1));
            } else {
                 if (strcmp(node->data.func.name, "wrap_i32") == 0) { free(node->data.func.name); node->data.func.name = strdup("wrap_int"); }
                 walk_and_specialize(node->data.func.body);
            }
            break;
        case AST_CALL:
            if (node->data.call.name) {
                if (strcmp(node->data.call.name, "wrap_i32") == 0) { free(node->data.call.name); node->data.call.name = strdup("wrap_int"); }
                if (strchr(node->data.call.name, '<')) {
                    char *type_name = strdup(node->data.call.name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg = lt + 1;
                        char *mangled = mangle_name(base, &arg, 1);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) register_specialization(mangled, specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? &generic->data.struct_decl.generic_params[0] : &generic->data.func.generic_params[0], &arg, 1));
                        }
                        free(node->data.call.name); node->data.call.name = strdup(mangled);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.call.name);
                    if (generic && (generic->type == AST_FUNC && generic->data.func.generic_param_count > 0)) {
                         char *arg = "int"; char *mangled = mangle_name(node->data.call.name, &arg, 1);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, &generic->data.func.generic_params[0], &arg, 1));
                         free(node->data.call.name); node->data.call.name = strdup(mangled);
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
                        char *base = type_name; char *arg = lt + 1;
                        char *mangled = mangle_name(base, &arg, 1);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) register_specialization(mangled, specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? &generic->data.struct_decl.generic_params[0] : &generic->data.func.generic_params[0], &arg, 1));
                        }
                        free(node->data.var_decl.type_name); node->data.var_decl.type_name = strdup(mangled);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.var_decl.type_name);
                    if (generic && (generic->type == AST_STRUCT_DECL && generic->data.struct_decl.generic_param_count > 0)) {
                         char *arg = "int"; char *mangled = mangle_name(node->data.var_decl.type_name, &arg, 1);
                         if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, &generic->data.struct_decl.generic_params[0], &arg, 1));
                         free(node->data.var_decl.type_name); node->data.var_decl.type_name = strdup(mangled);
                    }
                }
            } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                 if (node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "wrap_int") == 0 || strcmp(node->data.var_decl.init->data.call.name, "wrap_i32") == 0)) {
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
                    char *base = type_name; char *arg = lt + 1;
                    char *mangled = mangle_name(base, &arg, 1);
                    if (!is_specialized(mangled)) {
                        ASTNode *generic = monomorphization_lookup(base);
                        if (generic) register_specialization(mangled, specialize_node(generic, &generic->data.struct_decl.generic_params[0], &arg, 1));
                    }
                    free(node->data.struct_init.struct_name); node->data.struct_init.struct_name = strdup(mangled);
                }
                free(type_name);
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
        case AST_METHOD_CALL:
            walk_and_specialize(node->data.method_call.receiver);
            for (int i = 0; i < node->data.method_call.arg_count; i++) walk_and_specialize(node->data.method_call.args[i]);
            break;
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
        case AST_IMPL:
            for (int i = 0; i < node->data.impl_block.method_count; i++) walk_and_specialize(node->data.impl_block.methods[i]);
            break;
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
    SpecializationNode *curr = specializations;
    while (curr) {
        fprintf(out, "// Specialized: %s\n", curr->mangled_name);
        codegen_generate(curr->node, out, target, NULL);
        curr = curr->next;
    }
}
