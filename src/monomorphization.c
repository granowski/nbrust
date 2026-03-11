#include "ast.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct GenericRegistryNode {
    char *name;
    ASTNode *node;
    struct GenericRegistryNode *next;
} GenericRegistryNode;

static GenericRegistryNode *registry = NULL;

void monomorphization_register(ASTNode *node) {
    char *name = NULL;
    if (node->type == AST_FUNC) name = node->data.func.name;
    else if (node->type == AST_STRUCT_DECL) name = node->data.struct_decl.name;
    else if (node->type == AST_ENUM_DECL) name = node->data.enum_decl.name;
    
    if (!name) return;
    
    GenericRegistryNode *reg = malloc(sizeof(GenericRegistryNode));
    reg->name = strdup(name);
    reg->node = node; // Shallow copy, original AST owned by parser
    reg->next = registry;
    registry = reg;
}

ASTNode *monomorphization_lookup(const char *name) {
    GenericRegistryNode *curr = registry;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr->node;
        curr = curr->next;
    }
    return NULL;
}

// Deep clone with type substitution
static char *substitute_type(const char *type, char **params, char **args, int count) {
    if (!type) return NULL;
    for (int i = 0; i < count; i++) {
        if (strcmp(type, params[i]) == 0) return strdup(args[i]);
    }
    // Handle complex types like Vec<T> -> Vec<i32>
    // Simplified: just string replacement for now
    char *res = strdup(type);
    for (int i = 0; i < count; i++) {
        char *pos = strstr(res, params[i]);
        if (pos) {
            // Very simplified replacement
            int prefix_len = pos - res;
            int suffix_len = strlen(pos + strlen(params[i]));
            char *new_res = malloc(prefix_len + strlen(args[i]) + suffix_len + 1);
            strncpy(new_res, res, prefix_len);
            strcpy(new_res + prefix_len, args[i]);
            strcpy(new_res + prefix_len + strlen(args[i]), pos + strlen(params[i]));
            free(res);
            res = new_res;
        }
    }
    return res;
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count) {
    if (!node) return NULL;
    ASTNode *new_node = ast_new(node->type);
    memcpy(new_node, node, sizeof(ASTNode)); // Base copy

    switch (node->type) {
        case AST_FUNC:
            new_node->data.func.name = malloc(strlen(node->data.func.name) + 16);
            sprintf(new_node->data.func.name, "%s_spec", node->data.func.name); // Simplified mangling
            new_node->data.func.return_type = substitute_type(node->data.func.return_type, params, args, count);
            new_node->data.func.params = malloc(sizeof(ASTNode*) * node->data.func.param_count);
            for (int i = 0; i < node->data.func.param_count; i++) {
                new_node->data.func.params[i] = specialize_node(node->data.func.params[i], params, args, count);
            }
            new_node->data.func.body = specialize_node(node->data.func.body, params, args, count);
            new_node->data.func.generic_params = NULL;
            new_node->data.func.generic_bounds = NULL;
            new_node->data.func.generic_bounds_counts = NULL;
            new_node->data.func.generic_param_count = 0;
            new_node->data.func.where_clauses = NULL;
            new_node->data.func.where_clause_count = 0;
            break;
        case AST_PARAM:
            new_node->data.param.name = strdup(node->data.param.name);
            new_node->data.param.type_name = substitute_type(node->data.param.type_name, params, args, count);
            break;
        case AST_VAR_DECL:
            new_node->data.var_decl.name = strdup(node->data.var_decl.name);
            new_node->data.var_decl.type_name = substitute_type(node->data.var_decl.type_name, params, args, count);
            new_node->data.var_decl.init = specialize_node(node->data.var_decl.init, params, args, count);
            break;
        case AST_IDENT:
            new_node->data.ident.name = strdup(node->data.ident.name);
            break;
        case AST_BINOP:
            new_node->data.binop.op = strdup(node->data.binop.op);
            new_node->data.binop.left = specialize_node(node->data.binop.left, params, args, count);
            new_node->data.binop.right = specialize_node(node->data.binop.right, params, args, count);
            break;
        case AST_FOR_STMT:
            new_node->data.for_loop.var_name = strdup(node->data.for_loop.var_name);
            new_node->data.for_loop.iterable = specialize_node(node->data.for_loop.iterable, params, args, count);
            new_node->data.for_loop.body = specialize_node(node->data.for_loop.body, params, args, count);
            break;
        case AST_MATCH:
            new_node->data.match_stmt.expr = specialize_node(node->data.match_stmt.expr, params, args, count);
            new_node->data.match_stmt.arm_count = node->data.match_stmt.arm_count;
            new_node->data.match_stmt.arms = malloc(sizeof(ASTNode*) * node->data.match_stmt.arm_count);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                new_node->data.match_stmt.arms[i] = specialize_node(node->data.match_stmt.arms[i], params, args, count);
            }
            break;
        case AST_MATCH_ARM:
            new_node->data.match_arm.pattern = specialize_node(node->data.match_arm.pattern, params, args, count);
            new_node->data.match_arm.body = specialize_node(node->data.match_arm.body, params, args, count);
            break;
        case AST_BLOCK:
            new_node->data.block.count = node->data.block.count;
            new_node->data.block.statements = malloc(sizeof(ASTNode*) * node->data.block.count);
            for (int i = 0; i < node->data.block.count; i++) {
                new_node->data.block.statements[i] = specialize_node(node->data.block.statements[i], params, args, count);
            }
            break;
        case AST_ENUM_DECL:
            new_node->data.enum_decl.name = malloc(strlen(node->data.enum_decl.name) + 32);
            sprintf(new_node->data.enum_decl.name, "%s_%s", node->data.enum_decl.name, args[0]); // Simple mangling for now
            new_node->data.enum_decl.variants = malloc(sizeof(ASTNode*) * node->data.enum_decl.variant_count);
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                new_node->data.enum_decl.variants[i] = specialize_node(node->data.enum_decl.variants[i], params, args, count);
            }
            new_node->data.enum_decl.generic_params = NULL;
            new_node->data.enum_decl.generic_bounds = NULL;
            new_node->data.enum_decl.generic_bounds_counts = NULL;
            new_node->data.enum_decl.generic_param_count = 0;
            new_node->data.enum_decl.where_clauses = NULL;
            new_node->data.enum_decl.where_clause_count = 0;
            break;
        case AST_ENUM_VARIANT:
            new_node->data.enum_variant.name = strdup(node->data.enum_variant.name);
            new_node->data.enum_variant.fields = malloc(sizeof(ASTNode*) * node->data.enum_variant.field_count);
            for (int i = 0; i < node->data.enum_variant.field_count; i++) {
                new_node->data.enum_variant.fields[i] = specialize_node(node->data.enum_variant.fields[i], params, args, count);
            }
            break;
        case AST_STRUCT_DECL:
            new_node->data.struct_decl.name = malloc(strlen(node->data.struct_decl.name) + 32);
            sprintf(new_node->data.struct_decl.name, "%s_%s", node->data.struct_decl.name, args[0]);
            new_node->data.struct_decl.fields = malloc(sizeof(ASTNode*) * node->data.struct_decl.field_count);
            for (int i = 0; i < node->data.struct_decl.field_count; i++) {
                new_node->data.struct_decl.fields[i] = specialize_node(node->data.struct_decl.fields[i], params, args, count);
            }
            new_node->data.struct_decl.generic_params = NULL;
            new_node->data.struct_decl.generic_bounds = NULL;
            new_node->data.struct_decl.generic_bounds_counts = NULL;
            new_node->data.struct_decl.generic_param_count = 0;
            new_node->data.struct_decl.where_clauses = NULL;
            new_node->data.struct_decl.where_clause_count = 0;
            break;
        // ... handle other types ...
        default: break;
    }
    return new_node;
}

#include "codegen.h" // For Target

typedef struct SpecializationNode {
    char *mangled_name;
    ASTNode *node;
    struct SpecializationNode *next;
} SpecializationNode;

static SpecializationNode *specializations = NULL;

static void register_specialization(const char *mangled_name, ASTNode *node) {
    SpecializationNode *s = malloc(sizeof(SpecializationNode));
    s->mangled_name = strdup(mangled_name);
    s->node = node;
    s->next = specializations;
    specializations = s;
}

static int is_specialized(const char *mangled_name) {
    SpecializationNode *curr = specializations;
    while (curr) {
        if (strcmp(curr->mangled_name, mangled_name) == 0) return 1;
        curr = curr->next;
    }
    return 0;
}

static char *mangle_name(const char *base, char **args, int count) {
    char buf[512];
    strcpy(buf, base);
    for (int i = 0; i < count; i++) {
        strcat(buf, "_");
        // Sanitize arg name for C identifier (e.g., &str -> RefStr)
        char *arg = args[i];
        for (int j = 0; arg[j]; j++) {
            if (arg[j] == '&') strcat(buf, "Ref");
            else if (arg[j] == ' ') strcat(buf, "_");
            else if (arg[j] == '*') strcat(buf, "Ptr");
            else {
                int len = strlen(buf);
                buf[len] = arg[j];
                buf[len+1] = '\0';
            }
        }
    }
    return strdup(buf);
}

static void walk_and_specialize(ASTNode *node);

static void walk_and_specialize(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) walk_and_specialize(node->data.block.statements[i]);
            break;
        case AST_FUNC:
            walk_and_specialize(node->data.func.body);
            break;
        case AST_VAR_DECL:
            walk_and_specialize(node->data.var_decl.init);
            // Check if type is generic, e.g., Result<i32, String>
            if (node->data.var_decl.type_name && strchr(node->data.var_decl.type_name, '<')) {
                // ... handle type specialization ...
            }
            break;
        case AST_CALL: {
            // Check for Turbo-fish or inferred generic call
            // For now, let's assume turbofish was parsed into the name or something similar
            // This is a placeholder for real inference
            for (int i = 0; i < node->data.call.arg_count; i++) walk_and_specialize(node->data.call.args[i]);
            break;
        }
        case AST_WHILE:
            walk_and_specialize(node->data.while_loop.condition);
            walk_and_specialize(node->data.while_loop.body);
            break;
        case AST_FOR_STMT:
            walk_and_specialize(node->data.for_loop.iterable);
            walk_and_specialize(node->data.for_loop.body);
            break;
        case AST_MATCH:
            walk_and_specialize(node->data.match_stmt.expr);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                walk_and_specialize(node->data.match_stmt.arms[i]);
            }
            break;
        case AST_MATCH_ARM:
            walk_and_specialize(node->data.match_arm.pattern);
            walk_and_specialize(node->data.match_arm.body);
            break;
        case AST_BINOP:
            walk_and_specialize(node->data.binop.left);
            walk_and_specialize(node->data.binop.right);
            break;
        // ... more cases ...
        default: break;
    }
}

void monomorphization_run(ASTNode *root) {
    walk_and_specialize(root);
    
    // After walking, we should have a list of specializations to emit
    SpecializationNode *curr = specializations;
    Target target = { ARCH_X86_64, OS_MACOS, BACKEND_C }; // Dummy target
    while (curr) {
        printf("// Specialized: %s\n", curr->mangled_name);
        codegen_generate(curr->node, stdout, target, NULL);
        curr = curr->next;
    }
}
