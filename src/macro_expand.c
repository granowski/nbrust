#include "macro_expand.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct MacroDefinition {
    char *name;
    char *body;
    struct MacroDefinition *next;
} MacroDefinition;

static MacroDefinition *macro_list = NULL;

static void add_macro(const char *name, const char *body) {
    MacroDefinition *m = malloc(sizeof(MacroDefinition));
    m->name = strdup(name);
    m->body = strdup(body);
    m->next = macro_list;
    macro_list = m;
}

static void expand_node(ASTNode **node_ptr);

static ASTNode* expand_macro_call(ASTNode *node) {
    MacroDefinition *m = macro_list;
    while (m) {
        if (strcmp(m->name, node->data.macro_call.name) == 0) {
            // Check for dbg! macro
            if (strcmp(m->name, "dbg") == 0) {
                // dbg!($val:expr) => println!("Value = %d", $val)
                ASTNode *new_node = ast_new(AST_MACRO_CALL);
                new_node->data.macro_call.name = strdup("println");
                new_node->data.macro_call.args = malloc(sizeof(ASTNode*) * (node->data.macro_call.arg_count + 1));
                
                // Add a format string literal as the first argument
                ASTNode *fmt = ast_new(AST_STRING_LITERAL);
                fmt->data.string_literal.value = strdup("Value = %d");
                new_node->data.macro_call.args[0] = fmt;
                
                // Append original args
                for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                    new_node->data.macro_call.args[i+1] = node->data.macro_call.args[i];
                }
                new_node->data.macro_call.arg_count = node->data.macro_call.arg_count + 1;
                return new_node;
            }
            // Check for panic! macro
            if (strcmp(m->name, "panic") == 0) {
                // panic!($msg:expr) => { println!("Panic: %s", $msg); exit(1); }
                // For simplicity, we just return a call to panic in C
                ASTNode *new_node = ast_new(AST_CALL);
                new_node->data.call.name = strdup("panic");
                new_node->data.call.args = node->data.macro_call.args;
                new_node->data.call.arg_count = node->data.macro_call.arg_count;
                return new_node;
            }
            break;
        }
        m = m->next;
    }
    return node;
}

static void expand_node(ASTNode **node_ptr) {
    if (!node_ptr || !*node_ptr) return;
    ASTNode *node = *node_ptr;
    
    switch (node->type) {
        case AST_MACRO_RULES:
            add_macro(node->data.macro_rules.name, node->data.macro_rules.body_text);
            break;
        case AST_MACRO_CALL:
            *node_ptr = expand_macro_call(node);
            break;
        case AST_FUNC: expand_node(&node->data.func.body); break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) {
                expand_node(&node->data.block.statements[i]);
            }
            break;
        case AST_IF:
            expand_node(&node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) expand_node(&node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            expand_node(&node->data.while_loop.body);
            break;
        case AST_MOD:
            if (node->data.module.body) expand_node(&node->data.module.body);
            break;
        default: break;
    }
}

void macro_expand_run(ASTNode *root) {
    expand_node(&root);
}
