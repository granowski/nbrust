#include "type_checker.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SymbolTable *current_table = NULL;

static Type *parse_type_string(const char *type_name) {
    if (!type_name) return type_primitive(PRIM_I32);
    if (strcmp(type_name, "i32") == 0 || strcmp(type_name, "int") == 0) return type_primitive(PRIM_I32);
    if (strcmp(type_name, "i64") == 0 || strcmp(type_name, "long long") == 0) return type_primitive(PRIM_I64);
    if (strcmp(type_name, "u32") == 0 || strcmp(type_name, "unsigned int") == 0) return type_primitive(PRIM_U32);
    if (strcmp(type_name, "u64") == 0 || strcmp(type_name, "unsigned long long") == 0) return type_primitive(PRIM_U64);
    if (strcmp(type_name, "usize") == 0 || strcmp(type_name, "size_t") == 0) return type_primitive(PRIM_USIZE);
    if (strcmp(type_name, "isize") == 0 || strcmp(type_name, "ssize_t") == 0) return type_primitive(PRIM_ISIZE);
    if (strcmp(type_name, "bool") == 0) return type_primitive(PRIM_BOOL);
    if (strcmp(type_name, "&str") == 0) return type_primitive(PRIM_STR);
    if (strcmp(type_name, "void") == 0) return type_primitive(PRIM_VOID);
    
    if (type_name[0] == '&') {
        int is_mut = (strncmp(type_name + 1, "mut ", 4) == 0);
        return type_reference(parse_type_string(type_name + 1 + (is_mut ? 4 : 0)), is_mut);
    }
    
    if (type_name[0] == '*') {
        int is_mut = (strncmp(type_name + 1, "mut ", 4) == 0);
        return type_pointer(parse_type_string(type_name + 1 + (is_mut ? 4 : (strncmp(type_name+1, "const ", 6) == 0 ? 6 : 0))), is_mut);
    }

    // Check symbol table for structs/enums
    Symbol *s = symbol_table_lookup(current_table, type_name);
    if (s) return s->type;

    return type_struct(type_name);
}

static Type *check_node(ASTNode *node);

static Type *check_node(ASTNode *node) {
    if (!node) return type_primitive(PRIM_VOID);
    
    switch (node->type) {
        case AST_LITERAL:
            return type_primitive(PRIM_I32);
        case AST_BOOL_LITERAL:
            return type_primitive(PRIM_BOOL);
        case AST_STRING_LITERAL:
            return type_primitive(PRIM_STR);
        case AST_IDENT: {
            Symbol *s = symbol_table_lookup(current_table, node->data.ident.name);
            if (!s) {
                fprintf(stderr, "Type error: undefined identifier '%s'\n", node->data.ident.name);
                return type_new(TYPE_UNKNOWN);
            }
            return s->type;
        }
        case AST_VAR_DECL: {
            Type *t = parse_type_string(node->data.var_decl.type_name);
            if (node->data.var_decl.init) {
                Type *init_t = check_node(node->data.var_decl.init);
                if (!type_equals(t, init_t) && t->kind != TYPE_UNKNOWN && init_t->kind != TYPE_UNKNOWN) {
                    fprintf(stderr, "Type error: type mismatch in variable declaration '%s'. Expected %s, found %s\n", 
                            node->data.var_decl.name, type_to_string(t), type_to_string(init_t));
                }
            }
            symbol_table_insert(current_table, node->data.var_decl.name, t);
            return type_primitive(PRIM_VOID);
        }
        case AST_FUNC: {
            Type *ret_t = parse_type_string(node->data.func.return_type);
            Type **params = malloc(sizeof(Type*) * node->data.func.param_count);
            for (int i = 0; i < node->data.func.param_count; i++) {
                params[i] = parse_type_string(node->data.func.params[i]->data.param.type_name);
            }
            Type *func_t = type_function(ret_t, params, node->data.func.param_count);
            symbol_table_insert(current_table, node->data.func.name, func_t);
            
            SymbolTable *old_table = current_table;
            current_table = symbol_table_new(old_table);
            for (int i = 0; i < node->data.func.param_count; i++) {
                symbol_table_insert(current_table, node->data.func.params[i]->data.param.name, params[i]);
            }
            check_node(node->data.func.body);
            current_table = old_table;
            return func_t;
        }
        case AST_BLOCK: {
            for (int i = 0; i < node->data.block.count; i++) {
                check_node(node->data.block.statements[i]);
            }
            return type_primitive(PRIM_VOID);
        }
        case AST_BINOP: {
            Type *left = check_node(node->data.binop.left);
            Type *right = check_node(node->data.binop.right);
            if (!type_equals(left, right)) {
                fprintf(stderr, "Type error: binary operator '%s' applied to different types %s and %s\n", 
                        node->data.binop.op, type_to_string(left), type_to_string(right));
            }
            if (strcmp(node->data.binop.op, "==") == 0 || strcmp(node->data.binop.op, "<") == 0 || 
                strcmp(node->data.binop.op, ">") == 0 || strcmp(node->data.binop.op, "&&") == 0) {
                return type_primitive(PRIM_BOOL);
            }
            return left;
        }
        case AST_CALL: {
            Symbol *s = symbol_table_lookup(current_table, node->data.call.name);
            if (!s || s->type->kind != TYPE_FUNCTION) {
                // Heuristic for built-in or unknown functions
                return type_primitive(PRIM_I32);
            }
            return s->type->data.function.return_type;
        }
        case AST_IF: {
            check_node(node->data.if_stmt.condition);
            check_node(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) check_node(node->data.if_stmt.else_branch);
            return type_primitive(PRIM_VOID);
        }
        case AST_RETURN: {
            if (node->data.ret_stmt.value) return check_node(node->data.ret_stmt.value);
            return type_primitive(PRIM_VOID);
        }
        case AST_STRUCT_DECL: {
            Type *t = type_struct(node->data.struct_decl.name);
            symbol_table_insert(current_table, node->data.struct_decl.name, t);
            return type_primitive(PRIM_VOID);
        }
        case AST_ENUM_DECL: {
            Type *t = type_enum(node->data.enum_decl.name);
            symbol_table_insert(current_table, node->data.enum_decl.name, t);
            return type_primitive(PRIM_VOID);
        }
        case AST_TRAIT: {
            Type *t = type_trait(node->data.trait_decl.name);
            symbol_table_insert(current_table, node->data.trait_decl.name, t);
            return type_primitive(PRIM_VOID);
        }
        case AST_UNOP: {
            if (strcmp(node->data.unop.op, "&") == 0) {
                 return type_reference(check_node(node->data.unop.expr), 0);
            } else if (strcmp(node->data.unop.op, "&mut ") == 0) {
                 return type_reference(check_node(node->data.unop.expr), 1);
            } else if (strcmp(node->data.unop.op, "*") == 0) {
                 Type *inner = check_node(node->data.unop.expr);
                 if (inner->kind == TYPE_POINTER) return inner->data.pointer.inner;
                 if (inner->kind == TYPE_REFERENCE) return inner->data.reference.inner;
                 return type_new(TYPE_UNKNOWN);
            }
            return check_node(node->data.unop.expr);
        }
        default:
            return type_primitive(PRIM_VOID);
    }
}

void type_checker_run(ASTNode *root) {
    current_table = symbol_table_new(NULL);
    check_node(root);
}
