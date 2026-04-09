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
    fprintf(stderr, "DEBUG: check_node type=%d at line %d\n", node->type, node->line);
    
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
            if (strcmp(node->data.ident.name, "self") == 0) {
                Symbol *s = symbol_table_lookup(current_table, "self");
                if (s) {
                    result = s->type;
                    node->resolved_type = result;
                    break;
                }
            }
            if (strcmp(node->data.ident.name, "Self") == 0) {
                SymbolTable *curr = current_table;
                while (curr && curr->is_block) curr = curr->parent;
                if (curr && curr->name) {
                     result = type_struct(curr->name);
                     node->resolved_type = result;
                     break;
                }
            }
            Symbol *s = symbol_table_lookup_path(current_table, node->data.ident.name);
            if (!s) {
                // Try replacing Message::Quit with Message_Quit for lookup
                char *alt_name = strdup(node->data.ident.name);
                char *p = alt_name;
                while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                s = symbol_table_lookup_path(current_table, alt_name);
                if (!s) s = symbol_table_lookup(current_table, alt_name);
                free(alt_name);
            }
            if (!s) {
                // If not found as a path, try simple lookup (standard behavior for local variables)
                s = symbol_table_lookup(current_table, node->data.ident.name);
            }
            if (!s) {
                // Try global table as last resort
                SymbolTable *t = current_table;
                while (t->parent) t = t->parent;
                s = symbol_table_lookup(t, node->data.ident.name);
            }
            if (s && s->type && s->type->kind == TYPE_ENUM) {
                result = s->type;
                node->resolved_type = result;
                // Check if it's a unit variant like Option::None
                if (strstr(node->data.ident.name, "::")) {
                    char *copy = strdup(node->data.ident.name);
                    char *sep = strstr(copy, "::");
                    *sep = '\0';
                    char *variant = sep + 2;
                    // If it is indeed a variant of this enum, result is already the enum type
                    // but we can mark it for codegen if needed.
                    free(copy);
                }
                break;
            }
            if (!s) {
                result = type_new(TYPE_UNKNOWN);
            } else {
                result = s->type;
            }
            node->resolved_type = result;
            break;
        }
        case AST_VAR_DECL: {
            Type *t = NULL;
            if (node->data.var_decl.type_name) {
                char *tname = node->data.var_decl.type_name;
                if (strcmp(tname, "int") == 0) t = type_primitive(PRIM_I32);
                else if (strcmp(tname, "i32") == 0) t = type_primitive(PRIM_I32);
                else if (strcmp(tname, "i64") == 0) t = type_primitive(PRIM_I64);
                else if (strcmp(tname, "u32") == 0) t = type_primitive(PRIM_U32);
                else if (strcmp(tname, "u64") == 0) t = type_primitive(PRIM_U64);
                else if (strcmp(tname, "usize") == 0) t = type_primitive(PRIM_USIZE);
                else if (strcmp(tname, "char") == 0) t = type_primitive(PRIM_I8);
                else if (strcmp(tname, "bool") == 0) t = type_primitive(PRIM_BOOL);
                else t = parse_type_string(tname);
            }
            if (node->data.var_decl.init) {
                Type *init_t = check_node(node->data.var_decl.init);
                if (!t) t = init_t; // Simple type inference
                
                // If it's a call like Point::new(), use the returned type
                if (node->data.var_decl.init->type == AST_CALL || node->data.var_decl.init->type == AST_STRUCT_INIT) {
                    if (init_t) t = init_t;
                }

                // If t is Result<i32, &str> and init_t is Result_i32_Refchar, use the specialized type
                if (t && init_t && (t->kind == TYPE_ENUM || t->kind == TYPE_STRUCT) && (init_t->kind == TYPE_STRUCT || init_t->kind == TYPE_ENUM)) {
                    const char *target_name = (t->kind == TYPE_ENUM) ? t->data.enum_type.name : t->data.struct_type.name;
                    const char *init_name = (init_t->kind == TYPE_ENUM) ? init_t->data.enum_type.name : init_t->data.struct_type.name;
                    if (target_name && init_name && strstr(init_name, target_name)) {
                        t = init_t;
                    }
                }
            }
            if (!t) t = type_primitive(PRIM_I32); // Default
            symbol_table_insert(current_table, node->data.var_decl.name, t);
            result = t;
            node->resolved_type = t;
            // Ensure initializer also knows the type if it's a call
            if (node->data.var_decl.init) {
                node->data.var_decl.init->resolved_type = t;
            }
            break;
        }
        case AST_IMPL: {
            const char *struct_name = node->data.impl_block.struct_name;
            Symbol *struct_sym = symbol_table_lookup(current_table, struct_name);
            SymbolTable *impl_scope;
            
            if (struct_sym) {
                if (!struct_sym->scope) {
                    struct_sym->scope = symbol_table_new(current_table, struct_name);
                }
                impl_scope = struct_sym->scope;
            } else {
                impl_scope = symbol_table_new(current_table, struct_name);
                symbol_table_insert(current_table, struct_name, type_struct(struct_name));
                struct_sym = symbol_table_lookup(current_table, struct_name);
                if (struct_sym) struct_sym->scope = impl_scope;
            }
            
            SymbolTable *old_table = current_table;
            current_table = impl_scope;
            for (int i = 0; i < node->data.impl_block.method_count; i++) {
                ASTNode *method = node->data.impl_block.methods[i];
                if (method->type == AST_FUNC) {
                    // Pre-register method in impl scope
                    Type *ret_t = parse_type_string(method->data.func.return_type);
                    Type *func_t = type_function(ret_t, NULL, 0);
                    symbol_table_insert(impl_scope, method->data.func.name, func_t);
                    method->resolved_type = func_t;
                }
            }
            for (int i = 0; i < node->data.impl_block.method_count; i++) {
                ASTNode *method = node->data.impl_block.methods[i];
                if (method->type == AST_FUNC) {
                    SymbolTable *method_scope = symbol_table_new(current_table, method->data.func.name);
                    method_scope->is_block = 1; // It is a block scope for Self lookup
                    method->scope = method_scope;
                    SymbolTable *prev_table = current_table;
                    current_table = method_scope;
                    
                    for (int j = 0; j < method->data.func.param_count; j++) {
                        ASTNode *param = method->data.func.params[j];
                        if (param->type == AST_PARAM) {
                            Type *pt = NULL;
                            if (strcmp(param->data.param.name, "self") == 0) {
                                pt = type_reference(type_struct(struct_name), 0);
                            } else if (strcmp(param->data.param.name, "&self") == 0) {
                                pt = type_reference(type_struct(struct_name), 0);
                            } else {
                                pt = parse_type_string(param->data.param.type_name);
                            }
                            symbol_table_insert(current_table, param->data.param.name, pt);
                            param->resolved_type = pt;
                        }
                    }
                    
                    check_node(method->data.func.body);
                    current_table = prev_table;
                }
            }
            current_table = old_table;
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_FUNC: {
            Type *ret_t = parse_type_string(node->data.func.return_type);
            Type **params = malloc(sizeof(Type*) * node->data.func.param_count);
            for (int i = 0; i < node->data.func.param_count; i++) {
                params[i] = parse_type_string(node->data.func.params[i]->data.param.type_name);
                node->data.func.params[i]->resolved_type = params[i];
            }
            Type *func_t = type_function(ret_t, params, node->data.func.param_count);
            symbol_table_insert(current_table, node->data.func.name, func_t);
            node->resolved_type = func_t;

            SymbolTable *old_table = current_table;
            current_table = symbol_table_new(old_table, node->data.func.name);
            node->scope = current_table;
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
            current_table->is_block = 1; // Mark as block scope
            node->scope = current_table;
            Type *last_type = type_primitive(PRIM_VOID);
            for (int i = 0; i < node->data.block.count; i++) {
                last_type = check_node(node->data.block.statements[i]);
            }
            node->resolved_type = last_type;
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
            Type *iterable_t = check_node(node->data.for_loop.iterable);
            node->data.for_loop.iterable->resolved_type = iterable_t;
            
            // Try to determine the element type of the iterable
            Type *element_t = NULL;
            if (iterable_t && iterable_t->kind == TYPE_STRUCT) {
                // Heuristic: Check if the struct has a next() method or is a Vec
                if (strncmp(iterable_t->data.struct_type.name, "Vec_", 4) == 0) {
                    element_t = parse_type_string(iterable_t->data.struct_type.name + 4);
                } else if (strncmp(iterable_t->data.struct_type.name, "VecIter_", 8) == 0) {
                    element_t = parse_type_string(iterable_t->data.struct_type.name + 8);
                } else {
                    // Look up the struct symbol to get its scope
                    Symbol *struct_sym = symbol_table_lookup_path(current_table, iterable_t->data.struct_type.name);
                    if (struct_sym && struct_sym->scope) {
                        // Look for next() method in the struct's scope
                        Symbol *s = symbol_table_lookup(struct_sym->scope, "next");
                        if (s && s->type && s->type->kind == TYPE_FUNCTION) {
                            Type *ret_t = s->type->data.function.return_type;
                            if (ret_t && ret_t->kind == TYPE_ENUM && strncmp(ret_t->data.enum_type.name, "Option_", 7) == 0) {
                                element_t = parse_type_string(ret_t->data.enum_type.name + 7);
                            }
                        }
                    }
                }
            }
            if (!element_t) element_t = type_primitive(PRIM_I32); // Default fallback
            
            SymbolTable *old_table = current_table;
            current_table = symbol_table_new(old_table, "for_body");
            current_table->is_block = 1;
            node->scope = current_table;
            
            symbol_table_insert(current_table, node->data.for_loop.var_name, element_t);
            
            check_node(node->data.for_loop.body);
            
            current_table = old_table;
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_MATCH: {
            Type *expr_t = check_node(node->data.match_stmt.expr);
            if (node->data.match_stmt.expr->type == AST_IDENT) {
                node->data.match_stmt.expr->resolved_type = expr_t;
            }
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
            char *name = node->data.call.name;
            Symbol *s = symbol_table_lookup_path(current_table, name);
            if (!s) {
                // Try replacing Point::new with Point_new for lookup
                char *alt_name = strdup(name);
                char *p = alt_name;
                while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                s = symbol_table_lookup_path(current_table, alt_name);
                if (!s) s = symbol_table_lookup(current_table, alt_name);
                
                // Try split Struct_method
                if (!s && strchr(alt_name, '_')) {
                     char *copy = strdup(alt_name);
                     char *underscore = strchr(copy, '_');
                     *underscore = '\0';
                     Symbol *ss = symbol_table_lookup(current_table, copy);
                     if (ss && ss->scope) {
                          s = symbol_table_lookup(ss->scope, underscore + 1);
                     }
                     if (!s) {
                          // Try global search for struct
                          SymbolTable *t = current_table;
                          while (t->parent) t = t->parent;
                          ss = symbol_table_lookup(t, copy);
                          if (ss && ss->scope) {
                               s = symbol_table_lookup(ss->scope, underscore + 1);
                          }
                     }
                     free(copy);
                }
                free(alt_name);
            }
            if (!s) {
                s = symbol_table_lookup(current_table, node->data.call.name);
            }
            if (s && s->type && s->type->kind == TYPE_ENUM) {
                result = s->type;
            } else if (!s || !s->type || s->type->kind != TYPE_FUNCTION) {
                // Heuristic for built-in or unknown functions
                result = type_primitive(PRIM_I32);
                
                // Special case for Option::None and similar
                if (strstr(name, "::")) {
                    char *name_copy = strdup(name);
                    char *last_sep = strstr(name_copy, "::");
                    *last_sep = '\0';
                    Symbol *ss = symbol_table_lookup_path(current_table, name_copy);
                    if (ss && ss->type && ss->type->kind == TYPE_ENUM) {
                        result = ss->type;
                    }
                    free(name_copy);
                }
            } else {
                result = s->type->data.function.return_type;
                // If it's a static method call Point::new or Point_new, the result should be Point
                char *name_copy = strdup(node->data.call.name);
                char *last_sep = strstr(name_copy, "::");
                if (!last_sep) last_sep = strchr(name_copy, '_');
                
                if (last_sep && last_sep > name_copy) {
                    *last_sep = '\0';
                    Symbol *ss = symbol_table_lookup(current_table, name_copy);
                    if (!ss) {
                         SymbolTable *t = current_table;
                         while (t->parent) t = t->parent;
                         ss = symbol_table_lookup(t, name_copy);
                    }
                    if (ss && ss->type && (ss->type->kind == TYPE_STRUCT || ss->type->kind == TYPE_ENUM)) {
                        result = ss->type;
                    }
                }
                free(name_copy);
            }
            for (int i = 0; i < node->data.call.arg_count; i++) check_node(node->data.call.args[i]);
            node->resolved_type = result;
            break;
        }
        case AST_RETURN: {
            if (node->data.ret_stmt.value) {
                result = check_node(node->data.ret_stmt.value);
                // Propagate return type to value if it's generic
                if (current_table->parent && current_table->parent->name) {
                     Symbol *fs = symbol_table_lookup(current_table->parent, current_table->name);
                     if (fs && fs->type->kind == TYPE_FUNCTION) {
                          Type *ret_t = fs->type->data.function.return_type;
                          node->data.ret_stmt.value->resolved_type = ret_t;
                     }
                }
            }
            else result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_METHOD_CALL: {
            Type *receiver_t = check_node(node->data.method_call.receiver);
            // Ensure receiver IDENT has the same type
            if (node->data.method_call.receiver->type == AST_IDENT) {
                node->data.method_call.receiver->resolved_type = receiver_t;
                fprintf(stderr, "DEBUG: METHOD_CALL receiver '%s' resolved to kind %d\n", node->data.method_call.receiver->data.ident.name, receiver_t->kind);
            }
            Type *inner_t = receiver_t;
            if (receiver_t->kind == TYPE_REFERENCE) inner_t = receiver_t->data.reference.inner;
            else if (receiver_t->kind == TYPE_POINTER) inner_t = receiver_t->data.pointer.inner;
            
            result = type_primitive(PRIM_I32); // Default
            if (inner_t && inner_t->kind == TYPE_STRUCT) {
                Symbol *s = symbol_table_lookup(current_table, inner_t->data.struct_type.name);
                if (!s) {
                    // Try global table
                    SymbolTable *t = current_table;
                    while (t->parent) t = t->parent;
                    s = symbol_table_lookup(t, inner_t->data.struct_type.name);
                }
                if (s && s->scope) {
                    Symbol *m = symbol_table_lookup(s->scope, node->data.method_call.method_name);
                    if (m && m->type->kind == TYPE_FUNCTION) {
                        result = m->type->data.function.return_type;
                    }
                }
            }
            node->resolved_type = result;
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
            node->resolved_type = t;
            
            // Add a scope for the struct to hold methods
            SymbolTable *struct_scope = symbol_table_new(current_table, node->data.struct_decl.name);
            Symbol *s = symbol_table_lookup(current_table, node->data.struct_decl.name);
            if (s) s->scope = struct_scope;

            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_STRUCT_INIT: {
            const char *sname = node->data.struct_init.struct_name;
            if (sname && strcmp(sname, "Self") == 0) {
                 SymbolTable *curr = current_table;
                 while (curr && curr->is_block) curr = curr->parent;
                 if (curr && curr->name) sname = curr->name;
            }
            Symbol *s = symbol_table_lookup(current_table, sname);
            if (!s) {
                // Try variant lookup Message::Move -> Message_Move
                char *alt_name = strdup(sname);
                char *p = alt_name;
                while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                s = symbol_table_lookup(current_table, alt_name);
                free(alt_name);
            }
            Type *t = s ? s->type : type_struct(sname);
            for (int i = 0; i < node->data.struct_init.field_count; i++) {
                check_node(node->data.struct_init.fields[i]);
            }
            result = t;
            // Robustly set specialized name for generics
            char *type_str = type_to_string(t);
            if (t->kind == TYPE_STRUCT && strstr(type_str, "<")) {
                 char *lt = strchr(type_str, '<');
                 char *gt = strrchr(type_str, '>');
                 if (lt && gt) {
                      char *mangled = strdup(type_str);
                      char *mlt = strchr(mangled, '<');
                      char *mgt = strrchr(mangled, '>');
                      *mlt = '_'; *mgt = '\0';
                      // Align with C-style primitives
                      if (strstr(mangled, "_i32")) { /* already i32 */ }
                      else if (strstr(mangled, "_int")) { char *p = strstr(mangled, "_int"); strcpy(p, "_i32"); }
                      else if (strstr(mangled, "_i8")) { char *p = strstr(mangled, "_i8"); strcpy(p, "_char"); }
                      
                      if (node->data.struct_init.struct_name) free(node->data.struct_init.struct_name);
                      node->data.struct_init.struct_name = mangled;
                 }
            }
            break;
        }
        case AST_ENUM_DECL: {
            Type *t = type_enum(node->data.enum_decl.name);
            symbol_table_insert(current_table, node->data.enum_decl.name, t);
            
            // Register variants with prefix for flat lookup
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = node->data.enum_decl.variants[i];
                char buf[256];
                snprintf(buf, sizeof(buf), "%s_%s", node->data.enum_decl.name, variant->data.enum_variant.name);
                symbol_table_insert(current_table, buf, t);
            }
            node->resolved_type = t;
            
            // Create a scope for the enum to store its variants
            SymbolTable *enum_scope = symbol_table_new(current_table, node->data.enum_decl.name);
            Symbol *s = symbol_table_lookup(current_table, node->data.enum_decl.name);
            if (s) s->scope = enum_scope;
            
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = node->data.enum_decl.variants[i];
                symbol_table_insert(enum_scope, variant->data.enum_variant.name, t);
            }
            
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
        case AST_EXTERN_BLOCK: {
            for (int i = 0; i < node->data.extern_block.count; i++) {
                check_node(node->data.extern_block.items[i]);
            }
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_EXTERN_CRATE: {
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_MACRO_RULES: {
            result = type_primitive(PRIM_VOID);
            break;
        }
        case AST_GENERIC_TYPE: {
            result = type_primitive(PRIM_VOID); // Or something else
            break;
        }
        default:
            result = type_primitive(PRIM_VOID);
            break;
    }
    if (node) node->resolved_type = result;
    return result;
}

void type_checker_reset() {
    current_table = symbol_table_new(NULL, "crate");
}

void type_checker_run(ASTNode *root) {
    if (!current_table) {
        current_table = symbol_table_new(NULL, "crate");
    }
    check_node(root);
}
