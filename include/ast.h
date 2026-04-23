#ifndef AST_H
#define AST_H

typedef enum {
    AST_FUNC,
    AST_VAR_DECL,
    AST_LITERAL,
    AST_BINOP,
    AST_BLOCK,
    AST_IDENT,
    AST_IF,
    AST_WHILE,
    AST_RETURN,
    AST_BOOL_LITERAL,
    AST_PARAM,
    AST_CALL,
    AST_STRUCT_DECL,
    AST_STRUCT_INIT,
    AST_FIELD_INIT,
    AST_FIELD_ACCESS,
    AST_IMPL,
    AST_METHOD_CALL,
    AST_MACRO_CALL,
    AST_STRING_LITERAL,
    AST_TUPLE,
    AST_UNOP,
    AST_ENUM_DECL,
    AST_ENUM_VARIANT,
    AST_MATCH,
    AST_MATCH_ARM,
    AST_MATCH_STMT,
    AST_TRAIT,
    AST_FOR_STMT,
    AST_GENERIC_TYPE,
    AST_MOD,
    AST_USE,
    AST_EXTERN_BLOCK,
    AST_EXTERN_CRATE,
    AST_MACRO_RULES,
    AST_TYPE_ALIAS,
    AST_CONST,
    AST_TRAIT_IMPL,
    AST_CAST
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    struct Type *resolved_type; // Added for type-aware codegen
    struct SymbolTable *scope; // Added to store local scopes for codegen
    int line;
    int col;
    union {
        struct {
            char *name;
            struct ASTNode **params;
            int param_count;
            char *return_type;
            struct ASTNode *body;
            char **generic_params;
            struct ASTNode ***generic_bounds; // Array of arrays of trait bounds for each param
            int *generic_bounds_counts;
            int generic_param_count;
            int is_generic;
            int is_specialized;
            struct ASTNode **where_clauses;
            int where_clause_count;
        } func;
        struct {
            char *name;
            char *type_name;
        } param;
        struct {
            char *name;
            struct ASTNode *init;
            char *type_name;
            int is_mutable;
        } var_decl;
        struct {
            char *value;
            char *value2; // For range patterns: end of range
            int is_range; // 1 = .. (inclusive), 2 = ..= (inclusive end)
        } literal;
        struct {
            char *name;
        } ident;
        struct {
            char *op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binop;
        struct {
            struct ASTNode **statements;
            int count;
        } block;
        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } if_stmt;
        struct {
            struct ASTNode *condition;
            struct ASTNode *body;
        } while_loop;
        struct {
            struct ASTNode *value;
        } ret_stmt;
        struct {
            char *name;
            struct ASTNode **args;
            int arg_count;
        } call;
        struct {
            int value;
        } bool_literal;
        struct {
            char *name;
            struct ASTNode **fields;
            int field_count;
            char **generic_params;
            struct ASTNode ***generic_bounds;
            int *generic_bounds_counts;
            int generic_param_count;
            int is_generic;
            int is_specialized;
            struct ASTNode **where_clauses;
            int where_clause_count;
        } struct_decl;
        struct {
            char *struct_name;
            struct ASTNode **fields;
            int field_count;
        } struct_init;
        struct {
            char *name;
            struct ASTNode *value;
        } field_init;
        struct {
            struct ASTNode *receiver;
            char *field_name;
        } field_access;
        struct {
            char *struct_name;
            struct ASTNode **methods;
            int method_count;
            char **generic_params;
            int generic_param_count;
        } impl_block;
        struct {
            struct ASTNode *receiver;
            char *method_name;
            struct ASTNode **args;
            int arg_count;
        } method_call;
        struct {
            char *name;
            struct ASTNode **args;
            int arg_count;
        } macro_call;
        struct {
            char *value;
        } string_literal;
        struct {
            struct ASTNode **elements;
            int count;
        } tuple;
        struct {
            char *op;
            struct ASTNode *expr;
        } unop;
        struct {
            char *name;
            struct ASTNode **variants;
            int variant_count;
            char **generic_params;
            struct ASTNode ***generic_bounds;
            int *generic_bounds_counts;
            int generic_param_count;
            int is_generic;
            int is_specialized;
            struct ASTNode **where_clauses;
            int where_clause_count;
        } enum_decl;
        struct {
            char *name;
            ASTNodeType variant_type; // AST_PARAM (unit), AST_CALL (tuple), AST_STRUCT_DECL (struct)
            struct ASTNode **fields;
            int field_count;
        } enum_variant;
        struct {
            struct ASTNode *pattern;
            struct ASTNode *body;
            struct ASTNode *guard_expr;
            struct ASTNode *range_start;
            struct ASTNode *range_end;
            struct ASTNode **or_patterns;
            int or_pattern_count;
        } match_arm;
        struct {
            struct ASTNode *expr;
            struct ASTNode **arms;
            int arm_count;
        } match_stmt;
        struct {
            char *name;
            struct ASTNode **methods;
            int method_count;
        } trait_decl;
        struct {
            char *trait_name;
            char *struct_name;
            struct ASTNode **methods;
            int method_count;
            char **generic_params;
            struct ASTNode ***generic_bounds;
            int *generic_bounds_counts;
            int generic_param_count;
            int is_generic;
            int is_specialized;
            struct ASTNode **where_clauses;
            int where_clause_count;
        } trait_impl;
        struct {
            char *base_name;
            char **params;
            int param_count;
        } generic_type;
        struct {
            char *var_name;
            struct ASTNode *iterable;
            struct ASTNode *body;
        } for_loop;
        struct {
            char *name;
            struct ASTNode *body;
        } module;
        struct {
            char *path;
        } use_stmt;
        struct {
            char *abi;
            struct ASTNode **items;
            int count;
        } extern_block;
        struct {
            char *name;
        } extern_crate;
        struct {
            char *name;
            char *body_text;
        } macro_rules;
        struct {
            char *name;
            char *type_name;
        } type_alias;
        struct {
            char *name;
            char *type_name;
            struct ASTNode *value;
        } const_decl;
        struct {
            struct ASTNode *expr;
            char *type_name;
        } cast;
    } data;
} ASTNode;

ASTNode *ast_new_at(ASTNodeType type, int line, int col);
ASTNode *ast_new_old(ASTNodeType type);
#define ast_new(type) ast_new_old(type)
void ast_free(ASTNode *node);
ASTNode *ast_clone(ASTNode *node);

#endif
