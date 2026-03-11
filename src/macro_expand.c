#include "macro_expand.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct MacroDefinition {
    char *name;
    char *body;
    struct MacroDefinition *next;
} MacroDefinition;

static MacroDefinition *macros = NULL;

static void add_macro(const char *name, const char *body) {
    MacroDefinition *m = malloc(sizeof(MacroDefinition));
    m->name = strdup(name);
    m->body = strdup(body);
    m->next = macros;
    macros = m;
}

static void expand_node(ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case AST_MACRO_RULES:
            add_macro(node->data.macro_rules.name, node->data.macro_rules.body_text);
            break;
        case AST_MACRO_CALL: {
            MacroDefinition *m = macros;
            while (m) {
                if (strcmp(m->name, node->data.macro_call.name) == 0) {
                    // In a real compiler, we would parse the body and substitute tokens.
                    // For now, we'll replace it with a marker call or a block if we could.
                    // Since we can't easily change the node type in-place to a block without more infrastructure,
                    // we'll just log the expansion.
                    // printf("Expanding macro %s!\n", m->name);
                    break;
                }
                m = m->next;
            }
            break;
        }
        case AST_FUNC: expand_node(node->data.func.body); break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) {
                expand_node(node->data.block.statements[i]);
            }
            break;
        case AST_IF:
            expand_node(node->data.if_stmt.then_branch);
            if (node->data.if_stmt.else_branch) expand_node(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            expand_node(node->data.while_loop.body);
            break;
        default: break;
    }
}

void macro_expand_run(ASTNode *root) {
    expand_node(root);
}
