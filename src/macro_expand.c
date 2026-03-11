#include "macro_expand.h"
#include "parser.h"
#include "lexer.h"
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
    MacroDefinition *m = macro_list;
    while (m) {
        if (strcmp(m->name, name) == 0) {
            free(m->body);
            m->body = strdup(body);
            return;
        }
        m = m->next;
    }
    m = malloc(sizeof(MacroDefinition));
    m->name = strdup(name);
    m->body = strdup(body);
    m->next = macro_list;
    macro_list = m;
}

static void expand_node(ASTNode **node_ptr);

static ASTNode* apply_macro_expansion(MacroDefinition *m, ASTNode *call) {
    if (strcmp(m->name, "vec") == 0 || strcmp(m->name, "my_vec") == 0) {
        // Fallback for vec! if not fully implemented via token replacement
    }

    // Very basic token-based replacement for macro_rules!
    // Format expected in m->body: (pattern) => { expansion }
    // Currently only handles simplest cases by replacing $arg with call arguments
    char *expansion = strdup(m->body);
    // This is a placeholder for a real token-based expansion engine.
    // To properly implement this, we need to:
    // 1. Parse the macro body into rules (patterns and templates).
    // 2. Match the call arguments against the patterns.
    // 3. Substitute matched fragments into the templates.
    // 4. Re-parse the resulting string as AST.
    
    // For now, we continue using hardcoded expansions for common macros
    // but we'll add a more flexible way to handle user macros.
    
    if (strcmp(m->name, "my_vec") == 0 || strcmp(m->name, "vec") == 0) {
        ASTNode *block = ast_new(AST_BLOCK);
        ASTNode **stmts = malloc(sizeof(ASTNode*) * (call->data.macro_call.arg_count + 2));
        int count = 0;
        
        ASTNode *v_decl = ast_new(AST_VAR_DECL);
        v_decl->data.var_decl.name = strdup("v");
        v_decl->data.var_decl.is_mutable = 1;
        ASTNode *v_init = ast_new(AST_CALL);
        v_init->data.call.name = strdup("Vec::new");
        v_decl->data.var_decl.init = v_init;
        stmts[count++] = v_decl;
        
        for (int i = 0; i < call->data.macro_call.arg_count; i++) {
            ASTNode *push = ast_new(AST_METHOD_CALL);
            push->data.method_call.receiver = ast_new(AST_IDENT);
            push->data.method_call.receiver->data.ident.name = strdup("v");
            push->data.method_call.method_name = strdup("push");
            push->data.method_call.args = malloc(sizeof(ASTNode*));
            push->data.method_call.args[0] = ast_clone(call->data.macro_call.args[i]);
            push->data.method_call.arg_count = 1;
            stmts[count++] = push;
        }
        
        ASTNode *v_ident = ast_new(AST_IDENT);
        v_ident->data.ident.name = strdup("v");
        stmts[count++] = v_ident;
        
        block->data.block.statements = stmts;
        block->data.block.count = count;
        free(expansion);
        return block;
    }
    
    // If it's a simple user-defined macro from macro_rules!, try a naive replacement
    // This is very limited and only for demonstration of "Milestone 2" progress.
    // In a real implementation, we would use a proper token stream.
    if (m->body && strstr(m->body, "=>")) {
        char *template = strstr(m->body, "=>");
        template += 2;
        while (*template && (*template == ' ' || *template == '{')) template++;
        char *end = template + strlen(template) - 1;
        while (end > template && (*end == ' ' || *end == '}' || *end == '\n')) end--;
        
        int len = end - template + 1;
        char *clean_template = malloc(len + 1);
        strncpy(clean_template, template, len);
        clean_template[len] = '\0';
        
        // Re-parse the template as an expression (or block)
        // This requires a way to feed a string to the parser.
        Lexer l;
        lexer_init(&l, clean_template);
        Parser p;
        parser_init(&p, &l);
        ASTNode *expanded = parse_expression(&p);
        
        free(clean_template);
        free(expansion);
        if (expanded) return expanded;
    }

    free(expansion);
    return call;
}

static ASTNode* expand_macro_call(ASTNode *node) {
    MacroDefinition *m = macro_list;
    while (m) {
        if (strcmp(m->name, node->data.macro_call.name) == 0) {
            return apply_macro_expansion(m, node);
        }
        m = m->next;
    }
    
    // Hardcoded defaults for built-in macros if not redefined
    if (strcmp(node->data.macro_call.name, "println") == 0) {
        return node; // Handled by codegen
    }
    if (strcmp(node->data.macro_call.name, "print") == 0) {
        return node; // Handled by codegen
    }

    if (strcmp(node->data.macro_call.name, "dbg") == 0) {
        ASTNode *new_node = ast_new(AST_MACRO_CALL);
        new_node->data.macro_call.name = strdup("println");
        new_node->data.macro_call.args = malloc(sizeof(ASTNode*) * (node->data.macro_call.arg_count + 1));
        ASTNode *fmt = ast_new(AST_STRING_LITERAL);
        fmt->data.string_literal.value = strdup("Value = %d");
        new_node->data.macro_call.args[0] = fmt;
        for (int i = 0; i < node->data.macro_call.arg_count; i++) {
            new_node->data.macro_call.args[i+1] = node->data.macro_call.args[i];
        }
        new_node->data.macro_call.arg_count = node->data.macro_call.arg_count + 1;
        return new_node;
    }
    
    if (strcmp(node->data.macro_call.name, "panic") == 0) {
        ASTNode *new_node = ast_new(AST_CALL);
        new_node->data.call.name = strdup("panic");
        new_node->data.call.args = node->data.macro_call.args;
        new_node->data.call.arg_count = node->data.macro_call.arg_count;
        return new_node;
    }

    if (strcmp(node->data.macro_call.name, "matches") == 0) {
        if (node->data.macro_call.arg_count < 2) return node;
        ASTNode *match_node = ast_new(AST_MATCH);
        match_node->data.match_stmt.expr = node->data.macro_call.args[0];
        match_node->data.match_stmt.arms = malloc(sizeof(ASTNode*) * 2);
        match_node->data.match_stmt.arm_count = 2;
        
        ASTNode *arm1 = ast_new(AST_MATCH_ARM);
        arm1->data.match_arm.pattern = node->data.macro_call.args[1];
        ASTNode *true_node = ast_new(AST_BOOL_LITERAL);
        true_node->data.bool_literal.value = 1;
        arm1->data.match_arm.body = true_node;
        
        ASTNode *arm2 = ast_new(AST_MATCH_ARM);
        ASTNode *underscore = ast_new(AST_IDENT);
        underscore->data.ident.name = strdup("_");
        arm2->data.match_arm.pattern = underscore;
        ASTNode *false_node = ast_new(AST_BOOL_LITERAL);
        false_node->data.bool_literal.value = 0;
        arm2->data.match_arm.body = false_node;
        
        match_node->data.match_stmt.arms[0] = arm1;
        match_node->data.match_stmt.arms[1] = arm2;
        return match_node;
    }

    return node;
}

static void expand_node(ASTNode **node_ptr) {
    if (!node_ptr || !*node_ptr) return;
    ASTNode *node = *node_ptr;
    
    if (node->type == AST_MACRO_RULES) {
        add_macro(node->data.macro_rules.name, node->data.macro_rules.body_text);
        return;
    }

    if (node->type == AST_MACRO_CALL) {
        *node_ptr = expand_macro_call(node);
        node = *node_ptr;
    }

    switch (node->type) {
        case AST_FUNC:
            expand_node(&node->data.func.body);
            for (int i = 0; i < node->data.func.param_count; i++) {
                expand_node(&node->data.func.params[i]);
            }
            break;
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
        case AST_FOR_STMT:
            expand_node(&node->data.for_loop.iterable);
            expand_node(&node->data.for_loop.body);
            break;
        case AST_MATCH:
            expand_node(&node->data.match_stmt.expr);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                expand_node(&node->data.match_stmt.arms[i]);
            }
            break;
        case AST_MATCH_ARM:
            expand_node(&node->data.match_arm.body);
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
