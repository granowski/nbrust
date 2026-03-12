#include "borrow_checker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Borrow {
    char *var_name;
    int is_mut;
    int depth;
    struct Borrow *next;
} Borrow;

typedef struct VarState {
    char *name;
    int is_moved;
    int depth;
    struct VarState *next;
} VarState;

static VarState *current_vars = NULL;
static Borrow *current_borrows = NULL;
static int current_depth = 0;

static void push_var(const char *name) {
    VarState *v = malloc(sizeof(VarState));
    v->name = strdup(name);
    v->is_moved = 0;
    v->depth = current_depth;
    v->next = current_vars;
    current_vars = v;
}

static void push_borrow(const char *name, int is_mut) {
    Borrow *b = malloc(sizeof(Borrow));
    b->var_name = strdup(name);
    b->is_mut = is_mut;
    b->depth = current_depth;
    b->next = current_borrows;
    current_borrows = b;
}

static VarState *find_var(const char *name) {
    VarState *v = current_vars;
    while (v) {
        if (strcmp(v->name, name) == 0) return v;
        v = v->next;
    }
    return NULL;
}

static int count_borrows(const char *name, int *is_mut) {
    int count = 0;
    *is_mut = 0;
    Borrow *b = current_borrows;
    while (b) {
        if (strcmp(b->var_name, name) == 0) {
            count++;
            if (b->is_mut) *is_mut = 1;
        }
        b = b->next;
    }
    return count;
}

static void pop_scope() {
    while (current_vars && current_vars->depth == current_depth) {
        VarState *next = current_vars->next;
        free(current_vars->name);
        free(current_vars);
        current_vars = next;
    }
    while (current_borrows && current_borrows->depth == current_depth) {
        Borrow *next = current_borrows->next;
        free(current_borrows->var_name);
        free(current_borrows);
        current_borrows = next;
    }
    current_depth--;
}

static void check_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL:
            if (node->data.var_decl.init && node->data.var_decl.init->type == AST_IDENT) {
                const char *init_name = node->data.var_decl.init->data.ident.name;
                VarState *v = find_var(init_name);
                int is_mut_borrowed = 0;
                int borrow_count = count_borrows(init_name, &is_mut_borrowed);
                if (v && !borrow_count && !is_mut_borrowed) {
                    v->is_moved = 1;
                } else if (v) {
                    fprintf(stderr, "Borrow check error at %d:%d: cannot move '%s' because it is borrowed\n", node->line, node->col, init_name);
                }
            }
            push_var(node->data.var_decl.name);
            check_node(node->data.var_decl.init);
            break;
        case AST_IDENT: {
            VarState *v = find_var(node->data.ident.name);
            if (v && v->is_moved) {
                fprintf(stderr, "Borrow check error at %d:%d: use of moved value '%s'\n", node->line, node->col, v->name);
            }
            break;
        }
        case AST_UNOP:
            if (strcmp(node->data.unop.op, "&") == 0) {
                 if (node->data.unop.expr->type == AST_IDENT) {
                     const char *name = node->data.unop.expr->data.ident.name;
                     VarState *v = find_var(name);
                     if (v) {
                         int is_mut_borrowed = 0;
                         count_borrows(name, &is_mut_borrowed);
                         if (is_mut_borrowed) {
                             fprintf(stderr, "Borrow check error at %d:%d: cannot borrow '%s' as immutable because it is also borrowed as mutable\n", node->line, node->col, name);
                         }
                         push_borrow(name, 0);
                     }
                 }
            } else if (strcmp(node->data.unop.op, "&mut ") == 0 || strcmp(node->data.unop.op, "&mut") == 0) {
                 if (node->data.unop.expr->type == AST_IDENT) {
                     const char *name = node->data.unop.expr->data.ident.name;
                     VarState *v = find_var(name);
                     if (v) {
                         int is_mut_borrowed = 0;
                         int borrow_count = count_borrows(name, &is_mut_borrowed);
                         if (borrow_count > 0 || is_mut_borrowed) {
                             fprintf(stderr, "Borrow check error at %d:%d: cannot borrow '%s' as mutable more than once at a time\n", node->line, node->col, name);
                         }
                         push_borrow(name, 1);
                     }
                 }
            }
            check_node(node->data.unop.expr);
            break;
        case AST_FUNC: {
            VarState *old_vars = current_vars;
            Borrow *old_borrows = current_borrows;
            int old_depth = current_depth;
            current_vars = NULL; // New function scope
            current_borrows = NULL;
            current_depth = 0;
            for (int i = 0; i < node->data.func.param_count; i++) {
                push_var(node->data.func.params[i]->data.param.name);
            }
            check_node(node->data.func.body);
            // Free current scope
            while (current_vars) {
                VarState *next = current_vars->next;
                free(current_vars->name);
                free(current_vars);
                current_vars = next;
            }
            while (current_borrows) {
                Borrow *next = current_borrows->next;
                free(current_borrows->var_name);
                free(current_borrows);
                current_borrows = next;
            }
            current_vars = old_vars;
            current_borrows = old_borrows;
            current_depth = old_depth;
            break;
        }
        case AST_BLOCK:
            current_depth++;
            for (int i = 0; i < node->data.block.count; i++) {
                check_node(node->data.block.statements[i]);
            }
            pop_scope();
            break;
        case AST_CALL:
            for (int i = 0; i < node->data.call.arg_count; i++) {
                ASTNode *arg = node->data.call.args[i];
                if (arg->type == AST_IDENT) {
                    const char *name = arg->data.ident.name;
                    VarState *v = find_var(name);
                    // Move if it's not a reference (simple heuristic)
                    if (v) {
                        int is_mut_borrowed = 0;
                        int borrow_count = count_borrows(name, &is_mut_borrowed);
                        if (!borrow_count && !is_mut_borrowed) {
                             // Only move if it's not a primitive/copy type
                             // String literals (&str) are Copy in our current simplified model
                             // v->is_moved = 1;
                        } else {
                             fprintf(stderr, "Borrow check error at %d:%d: cannot move '%s' because it is borrowed\n", node->line, node->col, name);
                        }
                    }
                }
                check_node(arg);
            }
            break;
        case AST_IF:
            check_node(node->data.if_stmt.condition);
            check_node(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) check_node(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            check_node(node->data.while_loop.condition);
            check_node(node->data.while_loop.body);
            break;
        case AST_FOR_STMT:
            check_node(node->data.for_loop.iterable);
            check_node(node->data.for_loop.body);
            break;
        case AST_BINOP:
            if (strcmp(node->data.binop.op, "=") == 0 && node->data.binop.right->type == AST_IDENT) {
                const char *name = node->data.binop.right->data.ident.name;
                VarState *v = find_var(name);
                if (v) {
                    int is_mut_borrowed = 0;
                    int borrow_count = count_borrows(name, &is_mut_borrowed);
                    if (!borrow_count && !is_mut_borrowed) {
                        v->is_moved = 1;
                    } else {
                        fprintf(stderr, "Borrow check error at %d:%d: cannot move '%s' because it is borrowed\n", node->line, node->col, name);
                    }
                }
            }
            check_node(node->data.binop.left);
            check_node(node->data.binop.right);
            break;
        case AST_MACRO_CALL:
            for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                ASTNode *arg = node->data.macro_call.args[i];
                if (arg->type == AST_IDENT) {
                    VarState *v = find_var(arg->data.ident.name);
                    if (v && v->is_moved) {
                        fprintf(stderr, "Borrow check error at %d:%d: use of moved value '%s' in macro\n", node->line, node->col, v->name);
                    }
                }
                check_node(arg);
            }
            break;
        case AST_MATCH_ARM:
            check_node(node->data.match_arm.pattern);
            check_node(node->data.match_arm.body);
            break;
        case AST_MOD:
            if (node->data.module.body) check_node(node->data.module.body);
            break;
        case AST_RETURN:
            if (node->data.ret_stmt.value) check_node(node->data.ret_stmt.value);
            break;
        case AST_EXTERN_BLOCK:
            for (int i = 0; i < node->data.extern_block.count; i++) check_node(node->data.extern_block.items[i]);
            break;
        case AST_EXTERN_CRATE:
        case AST_MACRO_RULES:
        case AST_TYPE_ALIAS:
        case AST_CONST:
        case AST_TRAIT:
        case AST_GENERIC_TYPE:
        case AST_USE:
            break;
        case AST_TRAIT_IMPL:
            for (int i = 0; i < node->data.trait_impl.method_count; i++) check_node(node->data.trait_impl.methods[i]);
            break;
        case AST_IMPL:
            for (int i = 0; i < node->data.impl_block.method_count; i++) check_node(node->data.impl_block.methods[i]);
            break;
        default:
            // Skip other nodes for now
            break;
    }
}

void borrow_checker_run(ASTNode *root) {
    check_node(root);
}
