#include "parser.h"
#include <stdlib.h>
#include <string.h>

ASTNode *ast_new_at(ASTNodeType type, int line, int col) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

ASTNode *ast_new_old(ASTNodeType type) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    return node;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_FUNC:
            free(node->data.func.name);
            if (node->data.func.return_type) free(node->data.func.return_type);
            for (int i = 0; i < node->data.func.param_count; i++) {
                ast_free(node->data.func.params[i]);
            }
            if (node->data.func.params) free(node->data.func.params);
            ast_free(node->data.func.body);
            if (node->data.func.generic_params) {
                for (int i = 0; i < node->data.func.generic_param_count; i++) {
                    free(node->data.func.generic_params[i]);
                    if (node->data.func.generic_bounds && node->data.func.generic_bounds[i]) {
                        for (int j = 0; j < node->data.func.generic_bounds_counts[i]; j++) {
                            ast_free(node->data.func.generic_bounds[i][j]);
                        }
                        free(node->data.func.generic_bounds[i]);
                    }
                }
                free(node->data.func.generic_params);
                if (node->data.func.generic_bounds) free(node->data.func.generic_bounds);
                if (node->data.func.generic_bounds_counts) free(node->data.func.generic_bounds_counts);
            }
            if (node->data.func.where_clauses) {
                for (int i = 0; i < node->data.func.where_clause_count; i++) {
                    ast_free(node->data.func.where_clauses[i]);
                }
                free(node->data.func.where_clauses);
            }
            break;
        case AST_PARAM:
            free(node->data.param.name);
            free(node->data.param.type_name);
            break;
        case AST_VAR_DECL:
            free(node->data.var_decl.name);
            if (node->data.var_decl.type_name) free(node->data.var_decl.type_name);
            ast_free(node->data.var_decl.init);
            break;
        case AST_LITERAL:
            free(node->data.literal.value);
            break;
        case AST_IDENT:
            free(node->data.ident.name);
            break;
        case AST_BINOP:
            free(node->data.binop.op);
            ast_free(node->data.binop.left);
            ast_free(node->data.binop.right);
            break;
        case AST_BLOCK:
            if (node->data.block.statements) {
                for (int i = 0; i < node->data.block.count; i++) {
                    ast_free(node->data.block.statements[i]);
                }
                free(node->data.block.statements);
            }
            break;
        case AST_IF:
            ast_free(node->data.if_stmt.condition);
            ast_free(node->data.if_stmt.then_branch);
            ast_free(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            ast_free(node->data.while_loop.condition);
            ast_free(node->data.while_loop.body);
            break;
        case AST_FOR_STMT:
            free(node->data.for_loop.var_name);
            ast_free(node->data.for_loop.iterable);
            ast_free(node->data.for_loop.body);
            break;
        case AST_RETURN:
            ast_free(node->data.ret_stmt.value);
            break;
        case AST_CALL:
            free(node->data.call.name);
            for (int i = 0; i < node->data.call.arg_count; i++) {
                ast_free(node->data.call.args[i]);
            }
            if (node->data.call.args) free(node->data.call.args);
            break;
        case AST_STRUCT_DECL:
            free(node->data.struct_decl.name);
            for (int i = 0; i < node->data.struct_decl.field_count; i++) {
                ast_free(node->data.struct_decl.fields[i]);
            }
            if (node->data.struct_decl.fields) free(node->data.struct_decl.fields);
            if (node->data.struct_decl.generic_params) {
                for (int i = 0; i < node->data.struct_decl.generic_param_count; i++) {
                    free(node->data.struct_decl.generic_params[i]);
                    if (node->data.struct_decl.generic_bounds && node->data.struct_decl.generic_bounds[i]) {
                        for (int j = 0; j < node->data.struct_decl.generic_bounds_counts[i]; j++) {
                            ast_free(node->data.struct_decl.generic_bounds[i][j]);
                        }
                        free(node->data.struct_decl.generic_bounds[i]);
                    }
                }
                free(node->data.struct_decl.generic_params);
                if (node->data.struct_decl.generic_bounds) free(node->data.struct_decl.generic_bounds);
                if (node->data.struct_decl.generic_bounds_counts) free(node->data.struct_decl.generic_bounds_counts);
            }
            if (node->data.struct_decl.where_clauses) {
                for (int i = 0; i < node->data.struct_decl.where_clause_count; i++) {
                    ast_free(node->data.struct_decl.where_clauses[i]);
                }
                free(node->data.struct_decl.where_clauses);
            }
            break;
        case AST_STRUCT_INIT:
            free(node->data.struct_init.struct_name);
            for (int i = 0; i < node->data.struct_init.field_count; i++) {
                ast_free(node->data.struct_init.fields[i]);
            }
            if (node->data.struct_init.fields) free(node->data.struct_init.fields);
            break;
        case AST_FIELD_INIT:
            free(node->data.field_init.name);
            ast_free(node->data.field_init.value);
            break;
        case AST_FIELD_ACCESS:
            ast_free(node->data.field_access.receiver);
            free(node->data.field_access.field_name);
            break;
        case AST_IMPL:
            free(node->data.impl_block.struct_name);
            for (int i = 0; i < node->data.impl_block.method_count; i++) {
                ast_free(node->data.impl_block.methods[i]);
            }
            if (node->data.impl_block.methods) free(node->data.impl_block.methods);
            break;
        case AST_METHOD_CALL:
            ast_free(node->data.method_call.receiver);
            free(node->data.method_call.method_name);
            for (int i = 0; i < node->data.method_call.arg_count; i++) {
                ast_free(node->data.method_call.args[i]);
            }
            if (node->data.method_call.args) free(node->data.method_call.args);
            break;
        case AST_MACRO_CALL:
            free(node->data.macro_call.name);
            for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                ast_free(node->data.macro_call.args[i]);
            }
            if (node->data.macro_call.args) free(node->data.macro_call.args);
            break;
        case AST_STRING_LITERAL:
            free(node->data.string_literal.value);
            break;
        case AST_BOOL_LITERAL:
            break;
        case AST_UNOP:
            free(node->data.unop.op);
            ast_free(node->data.unop.expr);
            break;
        case AST_ENUM_DECL:
            free(node->data.enum_decl.name);
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                ast_free(node->data.enum_decl.variants[i]);
            }
            if (node->data.enum_decl.variants) free(node->data.enum_decl.variants);
            if (node->data.enum_decl.generic_params) {
                for (int i = 0; i < node->data.enum_decl.generic_param_count; i++) {
                    free(node->data.enum_decl.generic_params[i]);
                    if (node->data.enum_decl.generic_bounds && node->data.enum_decl.generic_bounds[i]) {
                        for (int j = 0; j < node->data.enum_decl.generic_bounds_counts[i]; j++) {
                            ast_free(node->data.enum_decl.generic_bounds[i][j]);
                        }
                        free(node->data.enum_decl.generic_bounds[i]);
                    }
                }
                free(node->data.enum_decl.generic_params);
                if (node->data.enum_decl.generic_bounds) free(node->data.enum_decl.generic_bounds);
                if (node->data.enum_decl.generic_bounds_counts) free(node->data.enum_decl.generic_bounds_counts);
            }
            if (node->data.enum_decl.where_clauses) {
                for (int i = 0; i < node->data.enum_decl.where_clause_count; i++) {
                    ast_free(node->data.enum_decl.where_clauses[i]);
                }
                free(node->data.enum_decl.where_clauses);
            }
            break;
        case AST_ENUM_VARIANT:
            free(node->data.enum_variant.name);
            for (int i = 0; i < node->data.enum_variant.field_count; i++) {
                ast_free(node->data.enum_variant.fields[i]);
            }
            if (node->data.enum_variant.fields) free(node->data.enum_variant.fields);
            break;
        case AST_MATCH:
            ast_free(node->data.match_stmt.expr);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                ast_free(node->data.match_stmt.arms[i]);
            }
            if (node->data.match_stmt.arms) free(node->data.match_stmt.arms);
            break;
        case AST_MATCH_ARM:
            ast_free(node->data.match_arm.pattern);
            ast_free(node->data.match_arm.body);
            break;
        case AST_TRAIT:
            free(node->data.trait_decl.name);
            for (int i = 0; i < node->data.trait_decl.method_count; i++) {
                ast_free(node->data.trait_decl.methods[i]);
            }
            if (node->data.trait_decl.methods) free(node->data.trait_decl.methods);
            break;
        case AST_GENERIC_TYPE:
            free(node->data.generic_type.base_name);
            for (int i = 0; i < node->data.generic_type.param_count; i++) {
                free(node->data.generic_type.params[i]);
            }
            if (node->data.generic_type.params) free(node->data.generic_type.params);
            break;
        case AST_MOD:
            free(node->data.module.name);
            ast_free(node->data.module.body);
            break;
        case AST_USE:
            free(node->data.use_stmt.path);
            break;
        case AST_EXTERN_BLOCK:
            if (node->data.extern_block.abi) free(node->data.extern_block.abi);
            for (int i = 0; i < node->data.extern_block.count; i++) {
                ast_free(node->data.extern_block.items[i]);
            }
            if (node->data.extern_block.items) free(node->data.extern_block.items);
            break;
        case AST_EXTERN_CRATE:
            if (node->data.extern_crate.name) free(node->data.extern_crate.name);
            break;
        case AST_MACRO_RULES:
            free(node->data.macro_rules.name);
            if (node->data.macro_rules.body_text) free(node->data.macro_rules.body_text);
            break;
        case AST_TYPE_ALIAS:
            free(node->data.type_alias.name);
            free(node->data.type_alias.type_name);
            break;
        case AST_CONST:
            free(node->data.const_decl.name);
            free(node->data.const_decl.type_name);
            ast_free(node->data.const_decl.value);
            break;
        case AST_TRAIT_IMPL:
            if (node->data.trait_impl.trait_name) free(node->data.trait_impl.trait_name);
            if (node->data.trait_impl.struct_name) free(node->data.trait_impl.struct_name);
            for (int i = 0; i < node->data.trait_impl.method_count; i++) {
                ast_free(node->data.trait_impl.methods[i]);
            }
            if (node->data.trait_impl.methods) free(node->data.trait_impl.methods);
            if (node->data.trait_impl.generic_params) {
                for (int i = 0; i < node->data.trait_impl.generic_param_count; i++) {
                    free(node->data.trait_impl.generic_params[i]);
                    if (node->data.trait_impl.generic_bounds && node->data.trait_impl.generic_bounds[i]) {
                        for (int j = 0; j < node->data.trait_impl.generic_bounds_counts[i]; j++) {
                            ast_free(node->data.trait_impl.generic_bounds[i][j]);
                        }
                        free(node->data.trait_impl.generic_bounds[i]);
                    }
                }
                free(node->data.trait_impl.generic_params);
                if (node->data.trait_impl.generic_bounds) free(node->data.trait_impl.generic_bounds);
                if (node->data.trait_impl.generic_bounds_counts) free(node->data.trait_impl.generic_bounds_counts);
            }
            if (node->data.trait_impl.where_clauses) {
                for (int i = 0; i < node->data.trait_impl.where_clause_count; i++) {
                    ast_free(node->data.trait_impl.where_clauses[i]);
                }
                free(node->data.trait_impl.where_clauses);
            }
            break;
        case AST_CAST:
            ast_free(node->data.cast.expr);
            free(node->data.cast.type_name);
            break;
    }
    free(node);
}

void parser_init(Parser *p, Lexer *l) {
    p->lexer = l;
    p->current = lexer_next_token(l);
    p->next = lexer_next_token(l);
}

#undef ast_new
#define ast_new(type) ast_new_at(type, p->current.line, p->current.col)

static char *parse_type(Parser *p);
ASTNode *parse_expression(Parser *p);
static ASTNode *parse_expression_no_struct(Parser *p);
ASTNode *parse_statement(Parser *p);
static ASTNode *parse_pattern(Parser *p);

// Generic parsing helpers
static void parse_generic_params_with_bounds(Parser *p, char ***names, ASTNode ****bounds, int **bounds_counts, int *count) {
    if (p->current.type != TOKEN_LT) return;
    consume(p, TOKEN_LT);
    *names = malloc(sizeof(char*) * 10);
    *bounds = malloc(sizeof(ASTNode**) * 10);
    *bounds_counts = malloc(sizeof(int) * 10);
    *count = 0;
    while (p->current.type != TOKEN_GT && p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_IDENT) {
            (*names)[*count] = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
            
            // Parse bounds if present: T: Bound1 + Bound2
            if (p->current.type == TOKEN_COLON) {
                consume(p, TOKEN_COLON);
                ASTNode **current_bounds = malloc(sizeof(ASTNode*) * 5);
                int current_count = 0;
                while (p->current.type != TOKEN_COMMA && p->current.type != TOKEN_GT && p->current.type != TOKEN_EOF) {
                    char *bound_name = parse_type(p);
                    ASTNode *bound = ast_new(AST_IDENT);
                    bound->data.ident.name = bound_name;
                    current_bounds[current_count++] = bound;
                    if (p->current.type == TOKEN_PLUS) consume(p, TOKEN_PLUS);
                    else break;
                }
                (*bounds)[*count] = current_bounds;
                (*bounds_counts)[*count] = current_count;
            } else {
                (*bounds)[*count] = NULL;
                (*bounds_counts)[*count] = 0;
            }
            (*count)++;
        }
        if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
    }
    consume(p, TOKEN_GT);
}

static void parse_where_clause(Parser *p, ASTNode ***clauses, int *count) {
    if (p->current.type != TOKEN_WHERE) return;
    consume(p, TOKEN_WHERE);
    *clauses = malloc(sizeof(ASTNode*) * 10);
    *count = 0;
    while (p->current.type != TOKEN_LBRACE && p->current.type != TOKEN_SEMICOLON && p->current.type != TOKEN_EOF) {
        // T: Bound
        char *type_name = parse_type(p);
        consume(p, TOKEN_COLON);
        while (p->current.type != TOKEN_COMMA && p->current.type != TOKEN_LBRACE && p->current.type != TOKEN_SEMICOLON && p->current.type != TOKEN_EOF) {
            char *bound_name = parse_type(p);
            ASTNode *clause = ast_new(AST_BINOP); // Using binop for Type: Bound for now
            clause->data.binop.op = strdup(":");
            ASTNode *left = ast_new(AST_IDENT);
            left->data.ident.name = strdup(type_name);
            ASTNode *right = ast_new(AST_IDENT);
            right->data.ident.name = bound_name;
            clause->data.binop.left = left;
            clause->data.binop.right = right;
            (*clauses)[(*count)++] = clause;
            if (p->current.type == TOKEN_PLUS) consume(p, TOKEN_PLUS);
            else break;
        }
        free(type_name);
        if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
        else break;
    }
}

void real_consume(Parser *p, TokenType type, int call_line, const char *type_name) {
    if (p->current.type == type) {
        token_free(p->current);
        p->current = p->next;
        p->next = lexer_next_token(p->lexer);
    } else {
        fprintf(stderr, "Unexpected token %s (type %d), expected %s (%d) at line %d (called from %d)\n", 
               p->current.text, p->current.type, type_name, type, p->current.line, call_line);
        exit(1);
    }
}

static char *parse_type(Parser *p) {
    char buf[256] = "";
    if (p->current.type == TOKEN_DYN) {
        strcat(buf, "dyn ");
        consume(p, TOKEN_DYN);
    }
    if (p->current.type == TOKEN_STAR) {
        consume(p, TOKEN_STAR);
        if (p->current.type == TOKEN_MUT) {
            consume(p, TOKEN_MUT);
        } else if (p->current.type == TOKEN_CONST) {
            consume(p, TOKEN_CONST);
        }
        char *inner = parse_type(p);
        sprintf(buf, "%s*", inner);
        free(inner);
    } else if (p->current.type == TOKEN_AMP) {
        strcat(buf, "&");
        consume(p, TOKEN_AMP);
        if (p->current.type == TOKEN_MUT) {
            strcat(buf, "mut ");
            consume(p, TOKEN_MUT);
        }
        char *inner = parse_type(p);
        strcat(buf, inner);
        free(inner);
    } else if (p->current.type == TOKEN_IDENT) {
        char *type_name = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
        while (p->current.type == TOKEN_COLON_COLON) {
            consume(p, TOKEN_COLON_COLON);
            if (p->current.type == TOKEN_IDENT) {
                char *next_part = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                char *fullname = malloc(strlen(type_name) + 2 + strlen(next_part) + 1);
                sprintf(fullname, "%s_%s", type_name, next_part);
                free(type_name);
                free(next_part);
                type_name = fullname;
            } else {
                break;
            }
        }
        
        // Map Rust primitive types to C-style types for the C backend
        if (strcmp(type_name, "i32") == 0) {
            strcat(buf, "int");
        } else if (strcmp(type_name, "i64") == 0) {
            strcat(buf, "long long");
        } else if (strcmp(type_name, "u32") == 0) {
            strcat(buf, "unsigned int");
        } else if (strcmp(type_name, "u64") == 0) {
            strcat(buf, "unsigned long long");
        } else if (strcmp(type_name, "usize") == 0) {
            strcat(buf, "size_t");
        } else if (strcmp(type_name, "isize") == 0) {
            strcat(buf, "ssize_t");
        } else if (strcmp(type_name, "str") == 0) {
            strcat(buf, "char");
        } else if (p->current.type == TOKEN_LT) {
            // Generic type or Box<T>
            consume(p, TOKEN_LT);
            char *ptype = parse_type(p);
            while (p->current.type == TOKEN_COMMA) {
                consume(p, TOKEN_COMMA);
                char *more = parse_type(p);
                char *new_ptype = malloc(strlen(ptype) + 2 + strlen(more) + 1);
                sprintf(new_ptype, "%s, %s", ptype, more);
                free(ptype);
                free(more);
                ptype = new_ptype;
            }
            consume(p, TOKEN_GT);
            char *full_type = malloc(strlen(type_name) + 2 + strlen(ptype) + 1);
            sprintf(full_type, "%s<%s>", type_name, ptype);
            strcat(buf, full_type);
            free(full_type);
            free(ptype);
            free(type_name);
        } else {
            strcat(buf, type_name);
            free(type_name);
        }
    } else if (p->current.type == TOKEN_SELF_LOWER) {
        strcat(buf, "self");
        consume(p, TOKEN_SELF_LOWER);
    } else if (p->current.type == TOKEN_SELF_UPPER) {
        strcat(buf, "Self");
        consume(p, TOKEN_SELF_UPPER);
    }
    return strdup(buf);
}

static ASTNode *parse_pattern(Parser *p) {
    if (p->current.type == TOKEN_UNDERSCORE) {
        ASTNode *node = ast_new(AST_IDENT);
        node->data.ident.name = strdup("_");
        consume(p, TOKEN_UNDERSCORE);
        return node;
    }
    
    if (p->current.type != TOKEN_IDENT) {
        return NULL;
    }
    
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    while (p->current.type == TOKEN_COLON_COLON) {
        consume(p, TOKEN_COLON_COLON);
        if (p->current.type == TOKEN_IDENT) {
            char *member = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
            char *fullname = malloc(strlen(name) + 2 + strlen(member) + 1);
            sprintf(fullname, "%s_%s", name, member);
            free(name);
            free(member);
            name = fullname;
        } else {
            break;
        }
    }
    
    if (p->current.type == TOKEN_LPAREN) {
        consume(p, TOKEN_LPAREN);
        ASTNode **args = malloc(sizeof(ASTNode*) * 10);
        int arg_count = 0;
        while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
            args[arg_count++] = parse_pattern(p);
            if (p->current.type == TOKEN_COMMA) {
                consume(p, TOKEN_COMMA);
            }
        }
        consume(p, TOKEN_RPAREN);
        ASTNode *node = ast_new(AST_CALL);
        node->data.call.name = name;
        node->data.call.args = args;
        node->data.call.arg_count = arg_count;
        return node;
    } else if (p->current.type == TOKEN_LBRACE) {
        consume(p, TOKEN_LBRACE);
        ASTNode **fields = malloc(sizeof(ASTNode*) * 10);
        int field_count = 0;
        while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
            char *fname = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
            ASTNode *val_pattern = NULL;
            if (p->current.type == TOKEN_COLON) {
                consume(p, TOKEN_COLON);
                val_pattern = parse_pattern(p);
            } else {
                // Shorthand: field name is also the binder
                val_pattern = ast_new(AST_IDENT);
                val_pattern->data.ident.name = strdup(fname);
            }
            ASTNode *field_init = ast_new(AST_FIELD_INIT);
            field_init->data.field_init.name = fname;
            field_init->data.field_init.value = val_pattern;
            fields[field_count++] = field_init;
            if (p->current.type == TOKEN_COMMA) {
                consume(p, TOKEN_COMMA);
            }
        }
        consume(p, TOKEN_RBRACE);
        ASTNode *node = ast_new(AST_STRUCT_INIT);
        node->data.struct_init.struct_name = name;
        node->data.struct_init.fields = fields;
        node->data.struct_init.field_count = field_count;
        return node;
    }
    
    ASTNode *node = ast_new(AST_IDENT);
    node->data.ident.name = name;
    return node;
}

ASTNode *parse_expression(Parser *p);

static ASTNode *parse_primary(Parser *p, int allow_struct_init) {
    if (p->current.type == TOKEN_UNSAFE) {
        consume(p, TOKEN_UNSAFE);
        return parse_primary(p, allow_struct_init); // Skip unsafe in expressions too
    }

    if (p->current.type == TOKEN_STAR) {
         // Check if this is a dereference or multiplication
         // In parse_primary, it's almost always a dereference unless it's a binary op, 
         // which is handled by parse_expression_precedence.
         const char *op = "*";
         consume(p, TOKEN_STAR);
         ASTNode *expr = parse_primary(p, allow_struct_init);
         ASTNode *node = ast_new(AST_UNOP);
         node->data.unop.op = strdup(op);
         node->data.unop.expr = expr;
         return node;
    }

    if (p->current.type == TOKEN_AMP) {
        char buf[32] = {0};
        strcat(buf, p->current.text);
        consume(p, TOKEN_AMP);
        if (p->current.type == TOKEN_MUT) {
            strcat(buf, "mut ");
            consume(p, TOKEN_MUT);
        }
        char *op = strdup(buf);
        ASTNode *expr = parse_primary(p, allow_struct_init); 
        ASTNode *node = ast_new(AST_UNOP);
        node->data.unop.op = op;
        node->data.unop.expr = expr;
        return node;
    }
    if (p->current.type == TOKEN_INT) {
        ASTNode *node = ast_new(AST_LITERAL);
        node->data.literal.value = strdup(p->current.text);
        consume(p, TOKEN_INT);
        return node;
    } else if (p->current.type == TOKEN_STRING) {
        ASTNode *node = ast_new(AST_STRING_LITERAL);
        node->data.string_literal.value = strdup(p->current.text);
        consume(p, TOKEN_STRING);
        return node;
    } else if (p->current.type == TOKEN_IDENT || p->current.type == TOKEN_UNDERSCORE || p->current.type == TOKEN_SELF_UPPER) {
        char *name = NULL;
        if (p->current.type == TOKEN_UNDERSCORE) {
            name = strdup("_");
            consume(p, TOKEN_UNDERSCORE);
        } else if (p->current.type == TOKEN_SELF_UPPER) {
            name = strdup("Self");
            consume(p, TOKEN_SELF_UPPER);
        } else {
            name = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
        }
        
        while (p->current.type == TOKEN_COLON_COLON) {
            consume(p, TOKEN_COLON_COLON);
            if (p->current.type == TOKEN_IDENT) {
                char *member = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                char *fullname = malloc(strlen(name) + 2 + strlen(member) + 1);
                sprintf(fullname, "%s_%s", name, member);
                free(name);
                free(member);
                name = fullname;
            } else {
                break;
            }
        }
        
        after_ident_label:
        if (p->current.type == TOKEN_BANG) {
            consume(p, TOKEN_BANG);
            ASTNode **args = malloc(sizeof(ASTNode*) * 10);
            int arg_count = 0;
            if (p->current.type == TOKEN_LPAREN || p->current.type == TOKEN_LBRACKET || p->current.type == TOKEN_LBRACE) {
                TokenType open = p->current.type;
                TokenType close = (open == TOKEN_LPAREN) ? TOKEN_RPAREN : (open == TOKEN_LBRACKET ? TOKEN_RBRACKET : TOKEN_RBRACE);
                consume(p, open);
                while (p->current.type != close && p->current.type != TOKEN_EOF) {
                    args[arg_count++] = parse_expression(p);
                    if (p->current.type == TOKEN_COMMA) {
                        consume(p, TOKEN_COMMA);
                    }
                }
                consume(p, close);
            }
            ASTNode *node = ast_new(AST_MACRO_CALL);
            node->data.macro_call.name = name;
            node->data.macro_call.args = args;
            node->data.macro_call.arg_count = arg_count;
            return node;
        }
        
        if (p->current.type == TOKEN_LPAREN) {
            consume(p, TOKEN_LPAREN);
            ASTNode **args = malloc(sizeof(ASTNode*) * 10);
            int arg_count = 0;
            while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                args[arg_count++] = parse_expression(p);
                if (p->current.type == TOKEN_COMMA) {
                    consume(p, TOKEN_COMMA);
                }
            }
            consume(p, TOKEN_RPAREN);
            ASTNode *node = ast_new(AST_CALL);
            node->data.call.name = name;
            node->data.call.args = args;
            node->data.call.arg_count = arg_count;
            return node;
        } else if (p->current.type == TOKEN_LBRACE && p->next.type == TOKEN_IDENT) {
            if (!allow_struct_init) {
                ASTNode *node = ast_new(AST_IDENT);
                node->data.ident.name = name;
                return node;
            }
            consume(p, TOKEN_LBRACE);
            ASTNode **fields = malloc(sizeof(ASTNode*) * 20);
            int field_count = 0;
            while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                char *fname = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                ASTNode *value = NULL;
                if (p->current.type == TOKEN_COLON) {
                    consume(p, TOKEN_COLON);
                    value = parse_expression(p);
                } else {
                    ASTNode *vnode = ast_new(AST_IDENT);
                    vnode->data.ident.name = strdup(fname);
                    value = vnode;
                }
                ASTNode *field = ast_new(AST_FIELD_INIT);
                field->data.field_init.name = fname;
                field->data.field_init.value = value;
                fields[field_count++] = field;
                if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
            }
            consume(p, TOKEN_RBRACE);
            ASTNode *node = ast_new(AST_STRUCT_INIT);
            node->data.struct_init.struct_name = name;
            node->data.struct_init.fields = fields;
            node->data.struct_init.field_count = field_count;
            return node;
        } else if (p->current.type == TOKEN_COLON_COLON) {
            consume(p, TOKEN_COLON_COLON);
            if (p->current.type == TOKEN_LT) {
                // Turbo-fish x::<i32>
                consume(p, TOKEN_LT);
                char *type_params = parse_type(p);
                consume(p, TOKEN_GT);
                while (p->current.type == TOKEN_COLON_COLON) {
                    consume(p, TOKEN_COLON_COLON);
                    if (p->current.type == TOKEN_IDENT) {
                        char *member = strdup(p->current.text);
                        consume(p, TOKEN_IDENT);
                        char *fullname = malloc(strlen(name) + 2 + strlen(member) + 1);
                        sprintf(fullname, "%s_%s", name, member);
                        free(name);
                        free(member);
                        name = fullname;
                    } else {
                        break;
                    }
                }
                // Re-check for call after turbofish
                if (p->current.type == TOKEN_LPAREN) {
                    consume(p, TOKEN_LPAREN);
                    ASTNode **args = malloc(sizeof(ASTNode*) * 10);
                    int arg_count = 0;
                    while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                        args[arg_count++] = parse_expression(p);
                        if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
                    }
                    consume(p, TOKEN_RPAREN);
                    ASTNode *node = ast_new(AST_CALL);
                    node->data.call.name = name;
                    node->data.call.args = args;
                    node->data.call.arg_count = arg_count;
                    return node;
                }
            } else if (p->current.type == TOKEN_IDENT) {
                char *member = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                char *fullname = malloc(strlen(name) + 2 + strlen(member) + 1);
                sprintf(fullname, "%s_%s", name, member);
                free(name);
                free(member);
                name = fullname;
                goto after_ident_label;
            }
            ASTNode *node = ast_new(AST_IDENT);
            node->data.ident.name = name;
            return node;
        } else if (p->current.type == TOKEN_LBRACE) {
            consume(p, TOKEN_LBRACE);
            ASTNode **fields = malloc(sizeof(ASTNode*) * 20);
            int field_count = 0;
            while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                char *fname = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                ASTNode *value = NULL;
                if (p->current.type == TOKEN_COLON) {
                    consume(p, TOKEN_COLON);
                    value = parse_expression(p);
                } else {
                    // Shorthand Point { x } -> x: x
                    value = ast_new(AST_IDENT);
                    value->data.ident.name = strdup(fname);
                }
                ASTNode *field = ast_new(AST_FIELD_INIT);
                field->data.field_init.name = fname;
                field->data.field_init.value = value;
                fields[field_count++] = field;
                if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
            }
            consume(p, TOKEN_RBRACE);
            ASTNode *node = ast_new(AST_STRUCT_INIT);
            node->data.struct_init.struct_name = name;
            node->data.struct_init.fields = fields;
            node->data.struct_init.field_count = field_count;
            return node;
        } else if (p->current.type == TOKEN_BANG) {
            consume(p, TOKEN_BANG);
            consume(p, TOKEN_LPAREN);
            ASTNode **args = malloc(sizeof(ASTNode*) * 10);
            int arg_count = 0;
            while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                args[arg_count++] = parse_expression(p);
                if (p->current.type == TOKEN_COMMA) {
                    consume(p, TOKEN_COMMA);
                }
            }
            consume(p, TOKEN_RPAREN);
            ASTNode *node = ast_new(AST_MACRO_CALL);
            node->data.macro_call.name = name;
            node->data.macro_call.args = args;
            node->data.macro_call.arg_count = arg_count;
            return node;
        } else {
            ASTNode *node = ast_new(AST_IDENT);
            node->data.ident.name = name;
            return node;
        }
    } else if (p->current.type == TOKEN_STRING) {
        ASTNode *node = ast_new(AST_STRING_LITERAL);
        node->data.string_literal.value = strdup(p->current.text);
        consume(p, TOKEN_STRING);
        return node;
    } else if (p->current.type == TOKEN_SELF_LOWER) {
        ASTNode *node = ast_new(AST_IDENT);
        node->data.ident.name = strdup("self");
        consume(p, TOKEN_SELF_LOWER);
        return node;
    } else if (p->current.type == TOKEN_TRUE) {
        ASTNode *node = ast_new(AST_BOOL_LITERAL);
        node->data.bool_literal.value = 1;
        consume(p, TOKEN_TRUE);
        return node;
    } else if (p->current.type == TOKEN_FALSE) {
        ASTNode *node = ast_new(AST_BOOL_LITERAL);
        node->data.bool_literal.value = 0;
        consume(p, TOKEN_FALSE);
        return node;
    } else if (p->current.type == TOKEN_LBRACE) {
        return parse_block(p);
    } else if (p->current.type == TOKEN_IF) {
        consume(p, TOKEN_IF);
        ASTNode *condition = parse_expression(p);
        ASTNode *then_branch = parse_block(p);
        ASTNode *else_branch = NULL;
        if (p->current.type == TOKEN_ELSE) {
            consume(p, TOKEN_ELSE);
            if (p->current.type == TOKEN_IF) {
                else_branch = parse_statement(p);
            } else {
                else_branch = parse_block(p);
            }
        }
        ASTNode *node = ast_new(AST_IF);
        node->data.if_stmt.condition = condition;
        node->data.if_stmt.then_branch = then_branch;
        node->data.if_stmt.else_branch = else_branch;
        return node;
    } else if (p->current.type == TOKEN_WHILE) {
        consume(p, TOKEN_WHILE);
        ASTNode *condition = parse_expression(p);
        ASTNode *body = parse_block(p);
        ASTNode *node = ast_new(AST_WHILE);
        node->data.while_loop.condition = condition;
        node->data.while_loop.body = body;
        return node;
    } else if (p->current.type == TOKEN_FOR) {
        consume(p, TOKEN_FOR);
        char *var_name = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
        consume(p, TOKEN_IN);
        ASTNode *iterable = parse_expression_no_struct(p);
        ASTNode *body = parse_block(p);

        /* Desugar: 
           {
             let mut __iter = IntoIterator::into_iter(iterable);
             while let Some(var_name) = __iter.next() {
                body
             }
           }
           Actually, simpler for now:
           {
             let mut __iter = iterable.into_iter();
             while { let __next = __iter.next(); match __next { Some(var_name) => true, None => false } } {
                let var_name = match __next { Some(v) => v };
                body
             }
           }
           Wait, 'while let' is better but nbrust might not have it.
           Let's use a simple while with match.
        */
        ASTNode *block = ast_new(AST_BLOCK);
        block->data.block.statements = malloc(sizeof(ASTNode*) * 2);
        block->data.block.count = 2;

        // let mut __iter = iterable.into_iter();
        ASTNode *iter_init = ast_new(AST_VAR_DECL);
        iter_init->data.var_decl.name = strdup("__iter");
        iter_init->data.var_decl.is_mutable = 1;
        
        ASTNode *into_iter_call = ast_new(AST_METHOD_CALL);
        into_iter_call->data.method_call.receiver = iterable;
        into_iter_call->data.method_call.method_name = strdup("into_iter");
        into_iter_call->data.method_call.args = NULL;
        into_iter_call->data.method_call.arg_count = 0;
        iter_init->data.var_decl.init = into_iter_call;
        block->data.block.statements[0] = iter_init;

        // while loop
        ASTNode *while_node = ast_new(AST_WHILE);
        
        // condition: { __next = __iter.next(); match __next { Some(_) => true, None => false } }
        // For simplicity in nbrust codegen, let's use a helper or specific AST_FOR_STMT if we want it to be clean.
        // But desugaring is more robust if we have the building blocks.
        // Let's use AST_FOR_STMT and handle it in codegen to avoid complex desugaring here.
        
        ASTNode *for_node = ast_new(AST_FOR_STMT);
        for_node->data.for_loop.var_name = var_name;
        for_node->data.for_loop.iterable = iterable;
        for_node->data.for_loop.body = body;
        return for_node;
    } else if (p->current.type == TOKEN_MATCH) {
        consume(p, TOKEN_MATCH);
        ASTNode *expr = parse_expression_no_struct(p);
        consume(p, TOKEN_LBRACE);
        ASTNode **arms = malloc(sizeof(ASTNode*) * 20);
        int arm_count = 0;
        while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
            ASTNode *pattern = parse_pattern(p);
            consume(p, TOKEN_FAT_ARROW);
            ASTNode *body;
            if (p->current.type == TOKEN_LBRACE) {
                body = parse_block(p);
            } else {
                body = parse_expression(p);
                if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
            }
            ASTNode *arm = ast_new(AST_MATCH_ARM);
            arm->data.match_arm.pattern = pattern;
            arm->data.match_arm.body = body;
            arms[arm_count++] = arm;
            if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
        }
        consume(p, TOKEN_RBRACE);
        ASTNode *node = ast_new(AST_MATCH);
        node->data.match_stmt.expr = expr;
        node->data.match_stmt.arms = arms;
        node->data.match_stmt.arm_count = arm_count;
        return node;
    } else if (p->current.type == TOKEN_LBRACE) {
        return parse_block(p);
    }
    return NULL;
}

static int get_precedence(TokenType type) {
    switch (type) {
        case TOKEN_DOT: return 8;
        case TOKEN_AMP: return 7;
        case TOKEN_EQUAL: return 1;
        case TOKEN_BAR_BAR: return 2;
        case TOKEN_AMP_AMP: return 3;
        case TOKEN_EQ_EQ:
        case TOKEN_BANG_EQ: return 4;
        case TOKEN_LT:
        case TOKEN_LT_EQ:
        case TOKEN_GT:
        case TOKEN_GT_EQ: return 5;
        case TOKEN_PLUS:
        case TOKEN_MINUS: return 6;
        case TOKEN_STAR:
        case TOKEN_SLASH: return 7;
        default: return 0;
    }
}

static ASTNode *parse_expression_precedence(Parser *p, int min_precedence, int allow_struct_init) {
    if (p->current.type == TOKEN_STAR && p->next.type != TOKEN_IDENT && p->next.type != TOKEN_LPAREN && p->next.type != TOKEN_LBRACE && p->next.type != TOKEN_LBRACKET) {
         // Heuristic: unary * (deref) has higher precedence than BINOP * (mul)
         // but let's handle it in parse_primary.
    }
    
    ASTNode *left = parse_primary(p, allow_struct_init);

    while (1) {
        if (p->current.type == TOKEN_AS) {
            consume(p, TOKEN_AS);
            char *type_name = parse_type(p);
            ASTNode *node = ast_new(AST_CAST);
            node->data.cast.expr = left;
            node->data.cast.type_name = type_name;
            left = node;
            continue;
        }

        int precedence = get_precedence(p->current.type);
        if (precedence == 0 || precedence < min_precedence) {
            break;
        }

        char *op = strdup(p->current.text);
        TokenType type = p->current.type;
        consume(p, type);

        if (type == TOKEN_DOT) {
            char *field_name = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
            if (p->current.type == TOKEN_LPAREN) {
                consume(p, TOKEN_LPAREN);
                ASTNode **args = malloc(sizeof(ASTNode*) * 10);
                int arg_count = 0;
                while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                    args[arg_count++] = parse_expression(p);
                    if (p->current.type == TOKEN_COMMA) {
                        consume(p, TOKEN_COMMA);
                    }
                }
                consume(p, TOKEN_RPAREN);
                ASTNode *node = ast_new(AST_METHOD_CALL);
                node->data.method_call.receiver = left;
                node->data.method_call.method_name = field_name;
                node->data.method_call.args = args;
                node->data.method_call.arg_count = arg_count;
                left = node;
            } else {
                ASTNode *node = ast_new(AST_FIELD_ACCESS);
                node->data.field_access.receiver = left;
                node->data.field_access.field_name = field_name;
                left = node;
            }
            free(op);
            continue;
        }

        ASTNode *right = parse_expression_precedence(p, precedence + 1, allow_struct_init);

        ASTNode *node = ast_new(AST_BINOP);
        node->data.binop.op = op;
        node->data.binop.left = left;
        node->data.binop.right = right;
        left = node;
    }

    return left;
}

static ASTNode *parse_expression_precedence(Parser *p, int min_precedence, int allow_struct_init);

ASTNode *parse_expression(Parser *p) {
    return parse_expression_precedence(p, 1, 1);
}

static ASTNode *parse_expression_no_struct(Parser *p) {
    return parse_expression_precedence(p, 1, 0);
}

static ASTNode *parse_primary(Parser *p, int allow_struct_init);

ASTNode *parse_statement(Parser *p) {
    if (p->current.type == TOKEN_UNSAFE) {
        consume(p, TOKEN_UNSAFE);
        ASTNode *stmt = parse_statement(p);
        // Maybe wrap in a special UNSAFE node if needed, 
        // but for now let's just mark the block or statement if we had a flag.
        return stmt;
    }
    
    if (p->current.type == TOKEN_IF || p->current.type == TOKEN_WHILE || p->current.type == TOKEN_MATCH || p->current.type == TOKEN_LBRACE) {
        return parse_primary(p, 1);
    }

    if (p->current.type == TOKEN_LET) {
        consume(p, TOKEN_LET);
        if (p->current.type == TOKEN_MUT) {
            consume(p, TOKEN_MUT);
        }
        
        ASTNode *pattern = parse_pattern(p);
        char *type_name = NULL;
        if (p->current.type == TOKEN_COLON) {
            consume(p, TOKEN_COLON);
            type_name = parse_type(p);
        }
        ASTNode *init = NULL;
        if (p->current.type == TOKEN_EQUAL) {
            consume(p, TOKEN_EQUAL);
            init = parse_expression(p);
        }
        consume(p, TOKEN_SEMICOLON);
        
        if (pattern->type == AST_IDENT) {
            ASTNode *node = ast_new(AST_VAR_DECL);
            node->data.var_decl.name = pattern->data.ident.name;
            node->data.var_decl.type_name = type_name;
            node->data.var_decl.init = init;
            // Note: pattern->data.ident.name is now owned by var_decl
            free(pattern);
            return node;
        } else {
            // General pattern in let: let (x, y) = ... or let Message::Write(text) = ...
            // This requires more complex codegen (like a match with one arm)
            // For now, let's just wrap it in a special node or reuse AST_VAR_DECL with a pattern
            ASTNode *node = ast_new(AST_VAR_DECL);
            node->data.var_decl.name = strdup("_pat"); // Dummy name
            node->data.var_decl.type_name = type_name;
            node->data.var_decl.init = init;
            // We'd need to extend AST_VAR_DECL to store the pattern
            return node;
        }
    } else if (p->current.type == TOKEN_TYPE) {
        return parse_type_alias(p);
    } else if (p->current.type == TOKEN_CONST) {
        return parse_const(p);
    } else if (p->current.type == TOKEN_RETURN) {
        consume(p, TOKEN_RETURN);
        ASTNode *value = NULL;
        if (p->current.type != TOKEN_SEMICOLON) {
            value = parse_expression(p);
        }
        consume(p, TOKEN_SEMICOLON);
        ASTNode *node = ast_new(AST_RETURN);
        node->data.ret_stmt.value = value;
        return node;
    } else {
        ASTNode *expr = parse_expression(p);
        if (p->current.type == TOKEN_SEMICOLON) {
            consume(p, TOKEN_SEMICOLON);
        }
        return expr;
    }
}

ASTNode *parse_type_alias(Parser *p) {
    consume(p, TOKEN_TYPE);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    consume(p, TOKEN_EQUAL);
    char *type_name = parse_type(p);
    consume(p, TOKEN_SEMICOLON);
    
    ASTNode *node = ast_new(AST_TYPE_ALIAS);
    node->data.type_alias.name = name;
    node->data.type_alias.type_name = type_name;
    return node;
}

ASTNode *parse_const(Parser *p) {
    consume(p, TOKEN_CONST);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    consume(p, TOKEN_COLON);
    char *type_name = parse_type(p);
    
    ASTNode *value = NULL;
    if (p->current.type == TOKEN_EQUAL) {
        consume(p, TOKEN_EQUAL);
        value = parse_expression(p);
    }
    consume(p, TOKEN_SEMICOLON);
    
    ASTNode *node = ast_new(AST_CONST);
    node->data.const_decl.name = name;
    node->data.const_decl.type_name = type_name;
    node->data.const_decl.value = value;
    return node;
}

ASTNode *parse_trait(Parser *p) {
    consume(p, TOKEN_TRAIT);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    consume(p, TOKEN_LBRACE);
    
    ASTNode **items = malloc(sizeof(ASTNode*) * 20);
    int item_count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_FN) {
            items[item_count++] = parse_function(p);
        } else if (p->current.type == TOKEN_TYPE) {
            items[item_count++] = parse_type_alias(p);
        } else if (p->current.type == TOKEN_CONST) {
            items[item_count++] = parse_const(p);
        } else {
             // Skip unexpected
             token_free(p->current);
             p->current = p->next;
             p->next = lexer_next_token(p->lexer);
        }
    }
    consume(p, TOKEN_RBRACE);
    
    ASTNode *node = ast_new(AST_TRAIT);
    node->data.trait_decl.name = name;
    node->data.trait_decl.methods = items;
    node->data.trait_decl.method_count = item_count;
    return node;
}

ASTNode *parse_mod(Parser *p) {
    consume(p, TOKEN_MOD);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    ASTNode *body = NULL;
    if (p->current.type == TOKEN_LBRACE) {
        body = parse_block(p);
    } else {
        consume(p, TOKEN_SEMICOLON);
    }
    
    ASTNode *node = ast_new(AST_MOD);
    node->data.module.name = name;
    node->data.module.body = body;
    return node;
}

ASTNode *parse_use(Parser *p) {
    consume(p, TOKEN_USE);
    char path[512] = "";
    while (p->current.type == TOKEN_IDENT || p->current.type == TOKEN_COLON_COLON || p->current.type == TOKEN_STAR) {
        strcat(path, p->current.text);
        consume(p, p->current.type);
        if (p->current.type == TOKEN_SEMICOLON) break;
    }
    consume(p, TOKEN_SEMICOLON);
    
    ASTNode *node = ast_new(AST_USE);
    node->data.use_stmt.path = strdup(path);
    return node;
}

ASTNode *parse_macro_rules(Parser *p) {
    consume(p, TOKEN_MACRO_RULES);
    consume(p, TOKEN_BANG);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    // We need to capture the raw body for expansion.
    // For now, let's capture the tokens between { and } as a string or a list of tokens.
    // Since we only have body_text, let's try to reconstruct it.
    char *body = malloc(4096);
    body[0] = '\0';
    
    if (p->current.type == TOKEN_LBRACE || p->current.type == TOKEN_LPAREN || p->current.type == TOKEN_LBRACKET) {
        TokenType open = p->current.type;
        TokenType close = (open == TOKEN_LBRACE) ? TOKEN_RBRACE : (open == TOKEN_LPAREN ? TOKEN_RPAREN : TOKEN_RBRACKET);
        
        strcat(body, p->current.text);
        consume(p, open);
        int nest_count = 1;
        while (nest_count > 0 && p->current.type != TOKEN_EOF) {
            strcat(body, " ");
            strcat(body, p->current.text);
            if (p->current.type == open) nest_count++;
            else if (p->current.type == close) nest_count--;
            consume(p, p->current.type);
        }
    }
    
    ASTNode *node = ast_new(AST_MACRO_RULES);
    node->data.macro_rules.name = name;
    node->data.macro_rules.body_text = body;
    return node;
}

ASTNode *parse_block(Parser *p) {
    consume(p, TOKEN_LBRACE);
    ASTNode *node = ast_new(AST_BLOCK);
    node->data.block.statements = malloc(sizeof(ASTNode*) * 100);
    node->data.block.count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        node->data.block.statements[node->data.block.count++] = parse_statement(p);
    }
    consume(p, TOKEN_RBRACE);
    return node;
}

ASTNode *parse_struct(Parser *p) {
    consume(p, TOKEN_STRUCT);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    ASTNode *node = ast_new(AST_STRUCT_DECL);
    node->data.struct_decl.name = name;

    parse_generic_params_with_bounds(p, &node->data.struct_decl.generic_params, &node->data.struct_decl.generic_bounds, &node->data.struct_decl.generic_bounds_counts, &node->data.struct_decl.generic_param_count);

    parse_where_clause(p, &node->data.struct_decl.where_clauses, &node->data.struct_decl.where_clause_count);

    if (p->current.type == TOKEN_SEMICOLON) {
        // Unit struct: struct Foo;
        consume(p, TOKEN_SEMICOLON);
        node->data.struct_decl.fields = NULL;
        node->data.struct_decl.field_count = 0;
        return node;
    }
    
    if (p->current.type == TOKEN_LPAREN) {
        // Tuple struct: struct Foo(i32, i32);
        consume(p, TOKEN_LPAREN);
        ASTNode **fields = malloc(sizeof(ASTNode*) * 10);
        int field_count = 0;
        while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
            ASTNode *field = ast_new(AST_PARAM);
            field->data.param.name = malloc(16);
            sprintf(field->data.param.name, "_%d", field_count);
            field->data.param.type_name = parse_type(p);
            fields[field_count++] = field;
            if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
        }
        consume(p, TOKEN_RPAREN);
        consume(p, TOKEN_SEMICOLON);
        node->data.struct_decl.fields = fields;
        node->data.struct_decl.field_count = field_count;
        return node;
    }

    consume(p, TOKEN_LBRACE);
    
    ASTNode **fields = malloc(sizeof(ASTNode*) * 20);
    int field_count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        char *fname = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
        consume(p, TOKEN_COLON);
        char *ftype = parse_type(p);
        
        ASTNode *field = ast_new(AST_PARAM); // Reuse AST_PARAM for struct fields
        field->data.param.name = fname;
        field->data.param.type_name = ftype;
        fields[field_count++] = field;
        
        if (p->current.type == TOKEN_COMMA) {
            consume(p, TOKEN_COMMA);
        }
    }
    consume(p, TOKEN_RBRACE);
    
    node->data.struct_decl.fields = fields;
    node->data.struct_decl.field_count = field_count;
    return node;
}

ASTNode *parse_impl(Parser *p) {
    consume(p, TOKEN_IMPL);
    
    char **generic_params = NULL;
    ASTNode ***generic_bounds = NULL;
    int *generic_bounds_counts = NULL;
    int generic_param_count = 0;
    parse_generic_params_with_bounds(p, &generic_params, &generic_bounds, &generic_bounds_counts, &generic_param_count);
    
    char *trait_name = NULL;
    char *struct_name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    if (p->current.type == TOKEN_LT) {
        // Skip generic args for now
        consume(p, TOKEN_LT);
        while (p->current.type != TOKEN_GT && p->current.type != TOKEN_EOF) {
            consume(p, p->current.type);
        }
        consume(p, TOKEN_GT);
    }
    
    if (p->current.type == TOKEN_FOR) {
        consume(p, TOKEN_FOR);
        trait_name = struct_name;
        struct_name = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
    } else if (p->current.type == TOKEN_IDENT) {
        // Handle impl Trait for Struct where Trait was already parsed as struct_name
        trait_name = struct_name;
        struct_name = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
        if (p->current.type == TOKEN_FOR) consume(p, TOKEN_FOR); // Should have been handled above, but just in case
    }

    ASTNode **where_clauses = NULL;
    int where_clause_count = 0;
    parse_where_clause(p, &where_clauses, &where_clause_count);
    
    consume(p, TOKEN_LBRACE);
    
    ASTNode **items = malloc(sizeof(ASTNode*) * 50);
    int item_count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_FN) {
            items[item_count++] = parse_function(p);
        } else if (p->current.type == TOKEN_TYPE) {
            items[item_count++] = parse_type_alias(p);
        } else if (p->current.type == TOKEN_CONST) {
            items[item_count++] = parse_const(p);
        } else {
             // Skip unexpected
             token_free(p->current);
             p->current = p->next;
             p->next = lexer_next_token(p->lexer);
        }
    }
    consume(p, TOKEN_RBRACE);
    
    if (trait_name) {
        ASTNode *node = ast_new(AST_TRAIT_IMPL);
        node->data.trait_impl.trait_name = trait_name;
        node->data.trait_impl.struct_name = struct_name;
        node->data.trait_impl.methods = items;
        node->data.trait_impl.method_count = item_count;
        node->data.trait_impl.generic_params = generic_params;
        node->data.trait_impl.generic_bounds = generic_bounds;
        node->data.trait_impl.generic_bounds_counts = generic_bounds_counts;
        node->data.trait_impl.generic_param_count = generic_param_count;
        node->data.trait_impl.where_clauses = where_clauses;
        node->data.trait_impl.where_clause_count = where_clause_count;
        return node;
    } else {
        ASTNode *node = ast_new(AST_IMPL);
        node->data.impl_block.struct_name = struct_name;
        node->data.impl_block.methods = items;
        node->data.impl_block.method_count = item_count;
        // impl blocks don't currently store generics/where clauses in ASTNode
        return node;
    }
}

ASTNode *parse_enum(Parser *p) {
    consume(p, TOKEN_ENUM);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    ASTNode *node = ast_new(AST_ENUM_DECL);
    node->data.enum_decl.name = name;

    parse_generic_params_with_bounds(p, &node->data.enum_decl.generic_params, &node->data.enum_decl.generic_bounds, &node->data.enum_decl.generic_bounds_counts, &node->data.enum_decl.generic_param_count);

    parse_where_clause(p, &node->data.enum_decl.where_clauses, &node->data.enum_decl.where_clause_count);
    
    consume(p, TOKEN_LBRACE);
    
    ASTNode **variants = malloc(sizeof(ASTNode*) * 50);
    int variant_count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        char *vname = strdup(p->current.text);
        consume(p, TOKEN_IDENT);
        
        ASTNode *variant = ast_new(AST_ENUM_VARIANT);
        variant->data.enum_variant.name = vname;
        
        if (p->current.type == TOKEN_LPAREN) {
            // Tuple variant
            consume(p, TOKEN_LPAREN);
            variant->data.enum_variant.variant_type = AST_CALL;
            ASTNode **fields = malloc(sizeof(ASTNode*) * 10);
            int field_count = 0;
            while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
                ASTNode *field = ast_new(AST_PARAM);
                field->data.param.type_name = parse_type(p);
                fields[field_count++] = field;
                if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
            }
            consume(p, TOKEN_RPAREN);
            variant->data.enum_variant.fields = fields;
            variant->data.enum_variant.field_count = field_count;
        } else if (p->current.type == TOKEN_LBRACE) {
            // Struct variant
            consume(p, TOKEN_LBRACE);
            variant->data.enum_variant.variant_type = AST_STRUCT_DECL;
            ASTNode **fields = malloc(sizeof(ASTNode*) * 20);
            int field_count = 0;
            while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
                char *fname = strdup(p->current.text);
                consume(p, TOKEN_IDENT);
                consume(p, TOKEN_COLON);
                char *ftype = parse_type(p);
                ASTNode *field = ast_new(AST_PARAM);
                field->data.param.name = fname;
                field->data.param.type_name = ftype;
                fields[field_count++] = field;
                if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
            }
            consume(p, TOKEN_RBRACE);
            variant->data.enum_variant.fields = fields;
            variant->data.enum_variant.field_count = field_count;
        } else {
            // Unit variant
            variant->data.enum_variant.variant_type = AST_PARAM;
            variant->data.enum_variant.fields = NULL;
            variant->data.enum_variant.field_count = 0;
        }
        
        variants[variant_count++] = variant;
        if (p->current.type == TOKEN_COMMA) consume(p, TOKEN_COMMA);
    }
    consume(p, TOKEN_RBRACE);
    
    node->data.enum_decl.variants = variants;
    node->data.enum_decl.variant_count = variant_count;
    return node;
}

ASTNode *parse_extern_block(Parser *p) {
    consume(p, TOKEN_EXTERN);
    char *abi = NULL;
    if (p->current.type == TOKEN_STRING) {
        abi = strdup(p->current.text);
        consume(p, TOKEN_STRING);
    }
    consume(p, TOKEN_LBRACE);
    
    ASTNode **items = malloc(sizeof(ASTNode*) * 50);
    int count = 0;
    while (p->current.type != TOKEN_RBRACE && p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_FN) {
            items[count++] = parse_function(p);
        } else if (p->current.type == TOKEN_SEMICOLON) {
            consume(p, TOKEN_SEMICOLON);
        } else {
            // Skip other unexpected tokens for now
            token_free(p->current);
            p->current = p->next;
            p->next = lexer_next_token(p->lexer);
        }
    }
    consume(p, TOKEN_RBRACE);
    
    ASTNode *node = ast_new(AST_EXTERN_BLOCK);
    node->data.extern_block.abi = abi;
    node->data.extern_block.items = items;
    node->data.extern_block.count = count;
    return node;
}

ASTNode *parse_extern_crate(Parser *p) {
    consume(p, TOKEN_EXTERN);
    consume(p, TOKEN_CRATE);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    consume(p, TOKEN_SEMICOLON);
    
    ASTNode *node = ast_new(AST_EXTERN_CRATE);
    node->data.extern_crate.name = name;
    return node;
}

ASTNode *parse_function(Parser *p) {
    consume(p, TOKEN_FN);
    char *name = strdup(p->current.text);
    consume(p, TOKEN_IDENT);
    
    ASTNode *node = ast_new(AST_FUNC);
    node->data.func.name = name;

    parse_generic_params_with_bounds(p, &node->data.func.generic_params, &node->data.func.generic_bounds, &node->data.func.generic_bounds_counts, &node->data.func.generic_param_count);

    consume(p, TOKEN_LPAREN);
    
    ASTNode **params = malloc(sizeof(ASTNode*) * 10);
    int param_count = 0;
    while (p->current.type != TOKEN_RPAREN && p->current.type != TOKEN_EOF) {
        if (p->current.type == TOKEN_AMP || p->current.type == TOKEN_SELF_LOWER) {
            // Simplified &self or self
            int is_ref = 0;
            if (p->current.type == TOKEN_AMP) {
                 consume(p, TOKEN_AMP);
                 is_ref = 1;
            }
            consume(p, TOKEN_SELF_LOWER);
            ASTNode *param = ast_new(AST_PARAM);
            param->data.param.name = strdup("self");
            param->data.param.type_name = strdup(is_ref ? "&self" : "self");
            params[param_count++] = param;
        } else {
            char *pname = strdup(p->current.text);
            consume(p, TOKEN_IDENT);
            consume(p, TOKEN_COLON);
            char *ptype = parse_type(p);
            
            ASTNode *param = ast_new(AST_PARAM);
            param->data.param.name = pname;
            param->data.param.type_name = ptype;
            params[param_count++] = param;
        }
        
        if (p->current.type == TOKEN_COMMA) {
            consume(p, TOKEN_COMMA);
        }
    }
    consume(p, TOKEN_RPAREN);
    
    char *return_type = NULL;
    if (p->current.type == TOKEN_ARROW) {
        consume(p, TOKEN_ARROW);
        return_type = parse_type(p);
    }
    
    node->data.func.params = params;
    node->data.func.param_count = param_count;
    node->data.func.return_type = return_type;
    
    parse_where_clause(p, &node->data.func.where_clauses, &node->data.func.where_clause_count);

    if (p->current.type == TOKEN_LBRACE) {
        ASTNode *body = parse_block(p);
        node->data.func.body = body;
    } else if (p->current.type == TOKEN_SEMICOLON) {
        consume(p, TOKEN_SEMICOLON);
        node->data.func.body = NULL;
    }
    return node;
}
