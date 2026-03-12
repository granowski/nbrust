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
    if (strcmp(type_name, "i8") == 0 || strcmp(type_name, "char") == 0) return type_primitive(PRIM_I8);
    if (strcmp(type_name, "u8") == 0 || strcmp(type_name, "unsigned char") == 0) return type_primitive(PRIM_U8);
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
    
    Type *result = type_primitive(PRIM_VOID);
    switch (node->type) {
        case AST_LITERAL:
            result = type_primitive(PRIM_I32);
            break;
        case AST_BOOL_LITERAL:
            result = type_primitive(PRIM_BOOL);
            break;
        case AST_STRING_LITERAL:
            result = type_primitive(PRIM_STR);
            break;
        case AST_IDENT: {
            Symbol *s = symbol_table_lookup_path(current_table, node->data.ident.name);
            if (!s) {
                // If not found as a path, try simple lookup (standard behavior for local variables)
                s = symbol_table_lookup(current_table, node->data.ident.name);
            }
            if (!s) {
                fprintf(stderr, "Type error: undefined identifier '%s'\n", node->data.ident.name);
                result = type_new(TYPE_UNKNOWN);
            } else {
                result = s->type;
            }
            break;
        }
        case AST_VAR_DECL: {
            Type *t = NULL;
            if (node->data.var_decl.type_name) {
                t = parse_type_string(node->data.var_decl.type_name);
            }
            if (node->data.var_decl.init) {
                Type *init_t = check_node(node->data.var_decl.init);
                if (!t) t = init_t; // Simple type inference
                else if (!type_equals(t, init_t) && t->kind != TYPE_UNKNOWN && init_t->kind != TYPE_UNKNOWN) {
                    fprintf(stderr, "Type error: type mismatch in variable declaration '%s'. Expected %s, found %s\n", 
                            node->data.var_decl.name, type_to_string(t), type_to_string(init_t));
                }
            }
            if (!t) t = type_primitive(PRIM_I32); // Default
            symbol_table_insert(current_table, node->data.var_decl.name, t);
            result = type_primitive(PRIM_VOID);
            break;
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
            current_table = symbol_table_new(old_table, node->data.func.name);
            for (int i = 0; i < node->data.func.param_count; i++) {
                symbol_table_insert(current_table, node->data.func.params[i]->data.param.name, params[i]);
            }
            check_node(node->data.func.body);
            current_table = old_table;
            result = func_t;
            break;
        }
        case AST_BLOCK: {
            SymbolTable *old_table = current_table;
            current_table = symbol_table_new(old_table, "block");
            Type *last_type = type_primitive(PRIM_VOID);
            for (int i = 0; i < node->data.block.count; i++) {
                last_type = check_node(node->data.block.statements[i]);
            }
            current_table = old_table;
            result = last_type;
            break;
        }
        case AST_IF: {
            check_node(node->data.if_stmt.condition);
            Type *then_t = check_node(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) {
                Type *else_t = check_node(node->data.if_stmt.else_branch);
                if (!type_equals(then_t, else_t)) {
                    fprintf(stderr, "Type error: if/else branches have incompatible types %s and %s\n", 
                            type_to_string(then_t), type_to_string(else_t));
                }
                result = then_t;
            } else {
                result = type_primitive(PRIM_VOID);
            }
            break;
        }
        case AST_WHILE: {
            check_node(node->data.while_loop.condition);
            check_node(node->data.while_loop.body);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_FOR_STMT: {
            // For now, assume it's valid if iterable is checked
            check_node(node->data.for_loop.iterable);
            // Ideally add var_name to a new scope
            check_node(node->data.for_loop.body);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_MATCH: {
            // Type *expr_t = check_node(node->data.match_stmt.expr);
            check_node(node->data.match_stmt.expr);
            Type *arm_t = type_primitive(PRIM_VOID);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                ASTNode *arm = node->data.match_stmt.arms[i];
                arm_t = check_node(arm->data.match_arm.body);
            }
            result = arm_t;
            break;
        }
        case AST_BINOP: {
            check_node(node->data.binop.left);
            check_node(node->data.binop.right);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_CALL: {
            Symbol *s = symbol_table_lookup_path(current_table, node->data.call.name);
            if (!s) {
                s = symbol_table_lookup(current_table, node->data.call.name);
            }
            if (!s || s->type->kind != TYPE_FUNCTION) {
                // Heuristic for built-in or unknown functions
                result = type_primitive(PRIM_I32);
            } else {
                result = s->type->data.function.return_type;
            }
            break;
        }
        case AST_RETURN: {
            if (node->data.ret_stmt.value) result = check_node(node->data.ret_stmt.value);
            else result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_MATCH_ARM: {
            // Pattern should be checked too, but for now just body
            result = check_node(node->data.match_arm.body);
            break;
        }
        case AST_STRUCT_DECL: {
            Type *t = type_struct(node->data.struct_decl.name);
            symbol_table_insert(current_table, node->data.struct_decl.name, t);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_ENUM_DECL: {
            Type *t = type_enum(node->data.enum_decl.name);
            symbol_table_insert(current_table, node->data.enum_decl.name, t);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_TRAIT: {
            Type *t = type_trait(node->data.trait_decl.name);
            symbol_table_insert(current_table, node->data.trait_decl.name, t);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_UNOP: {
            if (strcmp(node->data.unop.op, "&") == 0) {
                 result = type_reference(check_node(node->data.unop.expr), 0);
            } else if (strcmp(node->data.unop.op, "&mut ") == 0) {
                 result = type_reference(check_node(node->data.unop.expr), 1);
            } else if (strcmp(node->data.unop.op, "*") == 0) {
                 Type *inner = check_node(node->data.unop.expr);
                 if (inner->kind == TYPE_POINTER) result = inner->data.pointer.inner;
                 else if (inner->kind == TYPE_REFERENCE) result = inner->data.reference.inner;
                 else result = type_new(TYPE_UNKNOWN);
            } else {
                result = check_node(node->data.unop.expr);
            }
            break;
        }
        case AST_MOD: {
            SymbolTable *old_table = current_table;
            SymbolTable *mod_table = symbol_table_new(old_table, node->data.module.name);
            symbol_table_insert_scope(old_table, node->data.module.name, mod_table);
            
            if (node->data.module.body) {
                current_table = mod_table;
                check_node(node->data.module.body);
                current_table = old_table;
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_USE: {
            // For now, we don't fully resolve 'use' but we could record it in the current scope
            // Simplified: if it's use a::b::c; we might want to make 'c' available locally.
            char *last_part = strrchr(node->data.use_stmt.path, ':');
            if (last_part && last_part > node->data.use_stmt.path && *(last_part-1) == ':') {
                last_part++; // Skip ':'
                Symbol *s = symbol_table_lookup_path(current_table, node->data.use_stmt.path);
                if (s) {
                    if (s->scope) {
                        symbol_table_insert_scope(current_table, last_part, s->scope);
                    } else if (s->type) {
                        symbol_table_insert(current_table, last_part, s->type);
                    }
                }
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_TYPE_ALIAS: {
            Type *t = parse_type_string(node->data.type_alias.type_name);
            symbol_table_insert(current_table, node->data.type_alias.name, t);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_CONST: {
            Type *t = parse_type_string(node->data.const_decl.type_name);
            if (node->data.const_decl.value) {
                Type *val_t = check_node(node->data.const_decl.value);
                if (!type_equals(t, val_t) && t->kind != TYPE_UNKNOWN && val_t->kind != TYPE_UNKNOWN) {
                    fprintf(stderr, "Type error: type mismatch in const '%s'. Expected %s, found %s\n", 
                            node->data.const_decl.name, type_to_string(t), type_to_string(val_t));
                }
            }
            symbol_table_insert(current_table, node->data.const_decl.name, t);
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_TRAIT_IMPL: {
            // Find the struct in the symbol table
            Symbol *s = symbol_table_lookup(current_table, node->data.trait_impl.struct_name);
            if (s) {
                if (!s->scope) {
                    s->scope = symbol_table_new(current_table, node->data.trait_impl.struct_name);
                }
                SymbolTable *old_table = current_table;
                current_table = s->scope;
                
                // Register associated items in the struct's scope
                for (int i = 0; i < node->data.trait_impl.method_count; i++) {
                    check_node(node->data.trait_impl.methods[i]);
                }
                current_table = old_table;
            } else {
                // If struct not found, just check the methods in current scope
                for (int i = 0; i < node->data.trait_impl.method_count; i++) {
                    check_node(node->data.trait_impl.methods[i]);
                }
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_IMPL: {
            Symbol *s = symbol_table_lookup(current_table, node->data.impl_block.struct_name);
            if (s) {
                if (!s->scope) {
                    s->scope = symbol_table_new(current_table, node->data.impl_block.struct_name);
                }
                SymbolTable *old_table = current_table;
                current_table = s->scope;
                for (int i = 0; i < node->data.impl_block.method_count; i++) {
                    check_node(node->data.impl_block.methods[i]);
                }
                current_table = old_table;
            } else {
                for (int i = 0; i < node->data.impl_block.method_count; i++) {
                    check_node(node->data.impl_block.methods[i]);
                }
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_MACRO_CALL: {
            for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                check_node(node->data.macro_call.args[i]);
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        default:
            result = type_primitive(PRIM_VOID);
            break;
    }
    node->resolved_type = result;
    return result;
}

void type_checker_run(ASTNode *root) {
    if (!current_table) {
        current_table = symbol_table_new(NULL, "crate");
    }
    check_node(root);
}
