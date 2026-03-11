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
    if (strcmp(node->data.macro_call.name, "vec") == 0) {
        // Special case: vec![1, 2, 3] -> { let mut v = Vec::new(); v.push(1); v.push(2); v.push(3); v }
        ASTNode *block = ast_new(AST_BLOCK);
        ASTNode **stmts = malloc(sizeof(ASTNode*) * (node->data.macro_call.arg_count + 2));
        int count = 0;
        
        // let mut v = Vec::new();
        ASTNode *v_decl = ast_new(AST_VAR_DECL);
        v_decl->data.var_decl.name = strdup("v");
        v_decl->data.var_decl.is_mutable = 1;
        ASTNode *v_init = ast_new(AST_CALL);
        v_init->data.call.name = strdup("Vec::new");
        v_decl->data.var_decl.init = v_init;
        stmts[count++] = v_decl;
        
        for (int i = 0; i < node->data.macro_call.arg_count; i++) {
            ASTNode *push = ast_new(AST_METHOD_CALL);
            push->data.method_call.receiver = ast_new(AST_IDENT);
            push->data.method_call.receiver->data.ident.name = strdup("v");
            push->data.method_call.method_name = strdup("push");
            push->data.method_call.args = malloc(sizeof(ASTNode*));
            push->data.method_call.args[0] = ast_clone(node->data.macro_call.args[i]);
            push->data.method_call.arg_count = 1;
            stmts[count++] = push;
        }
        
        // v
        ASTNode *v_ident = ast_new(AST_IDENT);
        v_ident->data.ident.name = strdup("v");
        stmts[count++] = v_ident;
        
        block->data.block.statements = stmts;
        block->data.block.count = count;
        return block;
    }

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
            // Check for matches! macro
            if (strcmp(m->name, "matches") == 0) {
                // matches!($val, $pattern) => match $val { $pattern => true, _ => false }
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
            // Check for vec! macro
            if (strcmp(m->name, "vec") == 0) {
                // Simplified: vec![1, 2] => Vec::from_array([1, 2])
                ASTNode *new_node = ast_new(AST_CALL);
                new_node->data.call.name = strdup("Vec_from_array");
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
