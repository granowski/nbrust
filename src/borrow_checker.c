#include "borrow_checker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VarState {
    char *name;
    int is_moved;
    int borrow_count;
    int is_mut_borrowed;
    struct VarState *next;
} VarState;

static VarState *current_scope = NULL;

static void push_var(const char *name) {
    VarState *v = malloc(sizeof(VarState));
    v->name = strdup(name);
    v->is_moved = 0;
    v->borrow_count = 0;
    v->is_mut_borrowed = 0;
    v->next = current_scope;
    current_scope = v;
}

static VarState *find_var(const char *name) {
    VarState *v = current_scope;
    while (v) {
        if (strcmp(v->name, name) == 0) return v;
        v = v->next;
    }
    return NULL;
}

static void check_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_VAR_DECL:
            push_var(node->data.var_decl.name);
            check_node(node->data.var_decl.init);
            break;
        case AST_IDENT: {
            VarState *v = find_var(node->data.ident.name);
            if (v && v->is_moved) {
                fprintf(stderr, "Borrow check error: use of moved value '%s'\n", v->name);
            }
            break;
        }
        case AST_UNOP:
            if (strcmp(node->data.unop.op, "&") == 0) {
                 if (node->data.unop.expr->type == AST_IDENT) {
                     VarState *v = find_var(node->data.unop.expr->data.ident.name);
                     if (v) v->borrow_count++;
                 }
            } else if (strcmp(node->data.unop.op, "&mut ") == 0) {
                 if (node->data.unop.expr->type == AST_IDENT) {
                     VarState *v = find_var(node->data.unop.expr->data.ident.name);
                     if (v) {
                         if (v->borrow_count > 0 || v->is_mut_borrowed) {
                             fprintf(stderr, "Borrow check error: cannot borrow '%s' as mutable more than once at a time\n", v->name);
                         }
                         v->is_mut_borrowed = 1;
                     }
                 }
            }
            check_node(node->data.unop.expr);
            break;
        case AST_FUNC: {
            VarState *old_scope = current_scope;
            current_scope = NULL; // New function scope
            for (int i = 0; i < node->data.func.param_count; i++) {
                push_var(node->data.func.params[i]->data.param.name);
            }
            check_node(node->data.func.body);
            // Free current scope
            VarState *v = current_scope;
            while (v) {
                VarState *next = v->next;
                free(v->name);
                free(v);
                v = next;
            }
            current_scope = old_scope;
            break;
        }
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) {
                check_node(node->data.block.statements[i]);
            }
            break;
        case AST_CALL:
            for (int i = 0; i < node->data.call.arg_count; i++) {
                ASTNode *arg = node->data.call.args[i];
                if (arg->type == AST_IDENT) {
                    VarState *v = find_var(arg->data.ident.name);
                    // Simplified move semantics: move if not a primitive (heuristic)
                    if (v && !v->borrow_count && !v->is_mut_borrowed) {
                        // v->is_moved = 1; 
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
        case AST_BINOP:
            check_node(node->data.binop.left);
            check_node(node->data.binop.right);
            break;
        case AST_MATCH:
            check_node(node->data.match_stmt.expr);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                check_node(node->data.match_stmt.arms[i]->data.match_arm.body);
            }
            break;
        case AST_MOD:
            if (node->data.module.body) check_node(node->data.module.body);
            break;
        case AST_RETURN:
            if (node->data.ret_stmt.value) check_node(node->data.ret_stmt.value);
            break;
        default:
            // Skip other nodes for now
            break;
    }
}

void borrow_checker_run(ASTNode *root) {
    check_node(root);
}
