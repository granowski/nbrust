#include "codegen_armv6.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    char *name;
    int offset;
} Symbol;

typedef struct {
    char *label;
    char *value;
} StringConstant;

static Symbol symbol_table[100];
static int symbol_count = 0;
static int next_offset = -8; // First local at -8 (after fp, lr)

static StringConstant string_constants[100];
static int string_count = 0;

static void add_symbol(const char *name) {
    symbol_table[symbol_count].name = strdup(name);
    symbol_table[symbol_count].offset = next_offset;
    next_offset -= 4; // Assign 4 bytes per variable for ARMv6 (32-bit)
    symbol_count++;
}

static int get_symbol_offset(const char *name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            return symbol_table[i].offset;
        }
    }
    return 0;
}

static void clear_symbols() {
    for (int i = 0; i < symbol_count; i++) {
        free(symbol_table[i].name);
    }
    symbol_count = 0;
    next_offset = -8;
}

static const char* add_string_constant(const char *value) {
    for (int i = 0; i < string_count; i++) {
        if (strcmp(string_constants[i].value, value) == 0) {
            return string_constants[i].label;
        }
    }
    char label[32];
    sprintf(label, ".LC%d", string_count);
    string_constants[string_count].label = strdup(label);
    string_constants[string_count].value = strdup(value);
    return string_constants[string_count++].label;
}

static void clear_string_constants() {
    for (int i = 0; i < string_count; i++) {
        free(string_constants[i].label);
        free(string_constants[i].value);
    }
    string_count = 0;
}

static int label_count = 0;
static int next_label() {
    return label_count++;
}

static void codegen_node(ASTNode *node, FILE *out, Target target);

static void codegen_expr(ASTNode *node, FILE *out, Target target) {
    if (!node) return;
    switch (node->type) {
        case AST_LITERAL:
            fprintf(out, "    ldr r0, =%s\n", node->data.literal.value);
            break;
        case AST_BOOL_LITERAL:
            fprintf(out, "    mov r0, #%d\n", node->data.bool_literal.value ? 1 : 0);
            break;
        case AST_BINOP:
            if (strcmp(node->data.binop.op, "=") == 0) {
                codegen_expr(node->data.binop.right, out, target);
                if (node->data.binop.left->type == AST_IDENT) {
                    int offset = get_symbol_offset(node->data.binop.left->data.ident.name);
                    if (offset != 0) {
                        fprintf(out, "    str r0, [fp, #%d]\n", offset);
                    }
                } else if (node->data.binop.left->type == AST_UNOP && strcmp(node->data.binop.left->data.unop.op, "*") == 0) {
                    fprintf(out, "    push {r0}\n");
                    codegen_expr(node->data.binop.left->data.unop.expr, out, target);
                    fprintf(out, "    mov r1, r0\n");
                    fprintf(out, "    pop {r0}\n");
                    fprintf(out, "    str r0, [r1]\n");
                }
                break;
            }
            codegen_expr(node->data.binop.left, out, target);
            fprintf(out, "    push {r0}\n");
            codegen_expr(node->data.binop.right, out, target);
            fprintf(out, "    mov r1, r0\n");
            fprintf(out, "    pop {r0}\n");
            if (strcmp(node->data.binop.op, "+") == 0) {
                fprintf(out, "    add r0, r0, r1\n");
            } else if (strcmp(node->data.binop.op, "-") == 0) {
                fprintf(out, "    sub r0, r0, r1\n");
            } else if (strcmp(node->data.binop.op, "*") == 0) {
                fprintf(out, "    mul r0, r0, r1\n");
            } else if (strcmp(node->data.binop.op, "==") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    moveq r0, #1\n");
                fprintf(out, "    movne r0, #0\n");
            } else if (strcmp(node->data.binop.op, "!=") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    moveq r0, #0\n");
                fprintf(out, "    movne r0, #1\n");
            } else if (strcmp(node->data.binop.op, "<") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    movlt r0, #1\n");
                fprintf(out, "    movge r0, #0\n");
            } else if (strcmp(node->data.binop.op, "<=") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    movle r0, #1\n");
                fprintf(out, "    movgt r0, #0\n");
            } else if (strcmp(node->data.binop.op, ">") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    movgt r0, #1\n");
                fprintf(out, "    movle r0, #0\n");
            } else if (strcmp(node->data.binop.op, ">=") == 0) {
                fprintf(out, "    cmp r0, r1\n");
                fprintf(out, "    movge r0, #1\n");
                fprintf(out, "    movlt r0, #0\n");
            } else if (strcmp(node->data.binop.op, "&&") == 0) {
                int l_false = next_label();
                int l_end = next_label();
                fprintf(out, "    cmp r0, #0\n");
                fprintf(out, "    beq .L%d\n", l_false);
                codegen_expr(node->data.binop.right, out, target);
                fprintf(out, "    cmp r0, #0\n");
                fprintf(out, "    beq .L%d\n", l_false);
                fprintf(out, "    mov r0, #1\n");
                fprintf(out, "    b .L%d\n", l_end);
                fprintf(out, ".L%d:\n", l_false);
                fprintf(out, "    mov r0, #0\n");
                fprintf(out, ".L%d:\n", l_end);
            } else if (strcmp(node->data.binop.op, "||") == 0) {
                int l_true = next_label();
                int l_end = next_label();
                fprintf(out, "    cmp r0, #0\n");
                fprintf(out, "    bne .L%d\n", l_true);
                codegen_expr(node->data.binop.right, out, target);
                fprintf(out, "    cmp r0, #0\n");
                fprintf(out, "    bne .L%d\n", l_true);
                fprintf(out, "    mov r0, #0\n");
                fprintf(out, "    b .L%d\n", l_end);
                fprintf(out, ".L%d:\n", l_true);
                fprintf(out, "    mov r0, #1\n");
                fprintf(out, ".L%d:\n", l_end);
            }
            break;
        case AST_IDENT: {
            int offset = get_symbol_offset(node->data.ident.name);
            if (offset != 0) {
                fprintf(out, "    ldr r0, [fp, #%d]\n", offset);
            } else {
                fprintf(out, "    // Error: Unknown variable %s\n", node->data.ident.name);
            }
            break;
        }
        case AST_CALL: {
            if (strcmp(node->data.call.name, "Box::new") == 0) {
                fprintf(out, "    mov r0, #4\n");
                if (target.os == OS_MACOS) fprintf(out, "    bl _malloc\n");
                else fprintf(out, "    bl malloc\n");
                fprintf(out, "    push {r0}\n");
                codegen_expr(node->data.call.args[0], out, target);
                fprintf(out, "    mov r1, r0\n");
                fprintf(out, "    pop {r0}\n");
                fprintf(out, "    str r1, [r0]\n");
            } else if (strcmp(node->data.call.name, "println") == 0 || strcmp(node->data.call.name, "print") == 0) {
                for (int i = 0; i < node->data.call.arg_count && i < 4; i++) {
                    codegen_expr(node->data.call.args[i], out, target);
                    fprintf(out, "    push {r0}\n");
                }
                for (int i = (node->data.call.arg_count > 4 ? 4 : node->data.call.arg_count) - 1; i >= 0; i--) {
                    fprintf(out, "    pop {r%d}\n", i);
                }
                if (target.os == OS_MACOS) {
                    fprintf(out, "    bl _printf\n");
                } else {
                    fprintf(out, "    bl printf\n");
                }
                if (strcmp(node->data.call.name, "println") == 0) {
                    const char *nl_label = add_string_constant("\\n");
                    fprintf(out, "    ldr r0, =%s\n", nl_label);
                    if (target.os == OS_MACOS) {
                        fprintf(out, "    bl _printf\n");
                    } else {
                        fprintf(out, "    bl printf\n");
                    }
                }
            } else {
                // Pass arguments in r0-r3
                for (int i = 0; i < node->data.call.arg_count && i < 4; i++) {
                    codegen_expr(node->data.call.args[i], out, target);
                    fprintf(out, "    push {r0}\n");
                }
                // ARM EABI: arguments are in registers, let's reverse pop into r0-r3
                for (int i = (node->data.call.arg_count > 4 ? 4 : node->data.call.arg_count) - 1; i >= 0; i--) {
                    fprintf(out, "    pop {r%d}\n", i);
                }
                if (target.os == OS_MACOS) {
                    fprintf(out, "    bl _%s\n", node->data.call.name);
                } else {
                    fprintf(out, "    bl %s\n", node->data.call.name);
                }
            }
            break;
        }
        case AST_UNOP:
            if (strcmp(node->data.unop.op, "*") == 0) {
                codegen_expr(node->data.unop.expr, out, target);
                fprintf(out, "    ldr r0, [r0]\n");
            } else if (strcmp(node->data.unop.op, "&") == 0) {
                if (node->data.unop.expr->type == AST_IDENT) {
                    int offset = get_symbol_offset(node->data.unop.expr->data.ident.name);
                    if (offset != 0) {
                        fprintf(out, "    add r0, fp, #%d\n", offset);
                    } else {
                        fprintf(out, "    // Error: address-of unknown variable\n");
                    }
                }
            }
            break;
        case AST_MACRO_CALL:
            if (strcmp(node->data.macro_call.name, "println") == 0 || strcmp(node->data.macro_call.name, "print") == 0) {
                for (int i = 0; i < node->data.macro_call.arg_count && i < 4; i++) {
                    codegen_expr(node->data.macro_call.args[i], out, target);
                    fprintf(out, "    push {r0}\n");
                }
                for (int i = (node->data.macro_call.arg_count > 4 ? 4 : node->data.macro_call.arg_count) - 1; i >= 0; i--) {
                    fprintf(out, "    pop {r%d}\n", i);
                }
                if (target.os == OS_MACOS) {
                    fprintf(out, "    bl _printf\n");
                } else {
                    fprintf(out, "    bl printf\n");
                }
                if (strcmp(node->data.macro_call.name, "println") == 0) {
                    const char *nl_label = add_string_constant("\\n");
                    fprintf(out, "    ldr r0, =%s\n", nl_label);
                    if (target.os == OS_MACOS) {
                        fprintf(out, "    bl _printf\n");
                    } else {
                        fprintf(out, "    bl printf\n");
                    }
                }
            }
            break;
        case AST_STRING_LITERAL: {
            const char *label = add_string_constant(node->data.string_literal.value);
            fprintf(out, "    ldr r0, =%s\n", label);
            break;
        }
        default:
            fprintf(out, "    // Unsupported ARMv6 expression node type %d\n", node->type);
            break;
    }
}

static char current_func_name[256];

static void codegen_node(ASTNode *node, FILE *out, Target target) {
    if (!node) return;
    switch (node->type) {
        case AST_FUNC:
            strncpy(current_func_name, node->data.func.name, sizeof(current_func_name)-1);
            clear_symbols();
            
            if (target.os == OS_MACOS) {
                fprintf(out, ".global _%s\n", node->data.func.name);
                fprintf(out, "_%s:\n", node->data.func.name);
            } else {
                fprintf(out, ".global %s\n", node->data.func.name);
                fprintf(out, ".type %s, @function\n", node->data.func.name);
                fprintf(out, ".section \".text\"\n");
                fprintf(out, "%s:\n", node->data.func.name);
            }

            // Prologue: push {fp, lr}
            fprintf(out, "    push {fp, lr}\n");
            fprintf(out, "    add fp, sp, #4\n");
            fprintf(out, "    sub sp, sp, #32\n"); // Reserve space for locals

            // Handle parameters (up to 4 in r0-r3)
            for (int i = 0; i < node->data.func.param_count && i < 4; i++) {
                add_symbol(node->data.func.params[i]->data.param.name);
                int offset = get_symbol_offset(node->data.func.params[i]->data.param.name);
                fprintf(out, "    str r%d, [fp, #%d]\n", i, offset);
            }

            // Body
            codegen_node(node->data.func.body, out, target);

            // Epilogue
            fprintf(out, ".L%s_ret:\n", node->data.func.name);
            fprintf(out, "    sub sp, fp, #4\n");
            fprintf(out, "    pop {fp, pc}\n\n");
            break;

        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) {
                codegen_node(node->data.block.statements[i], out, target);
            }
            break;

        case AST_IF: {
            int l_else = next_label();
            int l_end = next_label();
            codegen_expr(node->data.if_stmt.condition, out, target);
            fprintf(out, "    cmp r0, #0\n");
            fprintf(out, "    beq .L%d\n", l_else);
            codegen_node(node->data.if_stmt.then_branch, out, target);
            fprintf(out, "    b .L%d\n", l_end);
            fprintf(out, ".L%d:\n", l_else);
            if (node->data.if_stmt.else_branch) {
                codegen_node(node->data.if_stmt.else_branch, out, target);
            }
            fprintf(out, ".L%d:\n", l_end);
            break;
        }

        case AST_WHILE: {
            int l_start = next_label();
            int l_end = next_label();
            fprintf(out, ".L%d:\n", l_start);
            codegen_expr(node->data.while_loop.condition, out, target);
            fprintf(out, "    cmp r0, #0\n");
            fprintf(out, "    beq .L%d\n", l_end);
            codegen_node(node->data.while_loop.body, out, target);
            fprintf(out, "    b .L%d\n", l_start);
            fprintf(out, ".L%d:\n", l_end);
            break;
        }

        case AST_RETURN:
            if (node->data.ret_stmt.value) {
                codegen_expr(node->data.ret_stmt.value, out, target);
            }
            fprintf(out, "    b .L%s_ret\n", current_func_name);
            break;

        case AST_VAR_DECL:
            if (node->data.var_decl.init) {
                codegen_expr(node->data.var_decl.init, out, target);
                add_symbol(node->data.var_decl.name);
                int offset = get_symbol_offset(node->data.var_decl.name);
                fprintf(out, "    str r0, [fp, #%d]\n", offset);
            } else {
                add_symbol(node->data.var_decl.name);
            }
            break;
        
        case AST_BINOP:
            if (strcmp(node->data.binop.op, "=") == 0) {
                if (node->data.binop.left->type == AST_IDENT) {
                    codegen_expr(node->data.binop.right, out, target);
                    int offset = get_symbol_offset(node->data.binop.left->data.ident.name);
                    if (offset != 0) {
                        fprintf(out, "    str r0, [fp, #%d]\n", offset);
                    } else {
                        fprintf(out, "    // Error: assignment to unknown variable\n");
                    }
                }
            } else {
                codegen_expr(node, out, target);
            }
            break;
        
        case AST_MATCH: {
            int l_end = next_label();
            codegen_expr(node->data.match_stmt.expr, out, target);
            // Result is a pointer to the enum struct. Tag is at offset 0.
            fprintf(out, "    ldr r0, [r0]\n"); 
            
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                ASTNode *arm = node->data.match_stmt.arms[i];
                int l_next = next_label();
                int l_body = next_label();
                fprintf(out, "    push {r0}\n"); 
                
                if (arm->data.match_arm.pattern->type == AST_IDENT && strcmp(arm->data.match_arm.pattern->data.ident.name, "_") == 0) {
                    fprintf(out, "    pop {r0}\n"); 
                    codegen_node(arm->data.match_arm.body, out, target);
                    fprintf(out, "    b .L%d\n", l_end);
                } else {
                    codegen_expr(arm->data.match_arm.pattern, out, target);
                    fprintf(out, "    mov r1, r0\n");
                    fprintf(out, "    pop {r0}\n"); 
                    
                    fprintf(out, "    cmp r0, r1\n");
                    fprintf(out, "    beq .Lbody_%d\n", l_body);
                    fprintf(out, "    b .L%d\n", l_next);
                    
                    fprintf(out, ".Lbody_%d:\n", l_body);
                    codegen_node(arm->data.match_arm.body, out, target);
                    fprintf(out, "    b .L%d\n", l_end);
                }
                fprintf(out, ".L%d:\n", l_next);
            }
            fprintf(out, ".L%d:\n", l_end);
            break;
        }

        case AST_FIELD_ACCESS:
            codegen_expr(node->data.field_access.receiver, out, target);
            if (strcmp(node->data.field_access.field_name, "tag") == 0) {
                fprintf(out, "    ldr r0, [r0]\n");
            } else {
                fprintf(out, "    ldr r0, [r0, #4]\n");
            }
            break;

        case AST_STRUCT_DECL:
        case AST_ENUM_DECL:
        case AST_TRAIT:
        case AST_IMPL:
        case AST_MOD:
        case AST_USE:
            fprintf(out, "// ARMv6: top-level declaration ignored in asm backend\n");
            break;

        case AST_LITERAL:
        case AST_IDENT:
        case AST_CALL:
        case AST_MACRO_CALL:
        case AST_UNOP:
            codegen_expr(node, out, target);
            break;

        default:
            fprintf(out, "    // Unsupported ARMv6 node type %d\n", node->type);
            break;
    }
}

void codegen_armv6_generate(ASTNode *node, FILE *out, Target target) {
    if (!node) return;
    
    if (node->type == AST_FUNC && strcmp(node->data.func.name, "main") == 0) {
        fprintf(out, "// ARMv6 Assembly generated by nbrust\n");
        fprintf(out, ".section \".text\"\n\n");
    }

    codegen_node(node, out, target);

    if (string_count > 0) {
        fprintf(out, "\n.section \".rodata\"\n");
        for (int i = 0; i < string_count; i++) {
            fprintf(out, "%s:\n", string_constants[i].label);
            fprintf(out, "    .asciz \"%s\"\n", string_constants[i].value);
        }
        clear_string_constants();
    }
}
