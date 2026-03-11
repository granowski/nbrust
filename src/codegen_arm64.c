#include "codegen_arm64.h"
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
static int next_offset = -16; // First local at -16 (after x29, x30)

static StringConstant string_constants[100];
static int string_count = 0;

static void add_symbol(const char *name) {
    symbol_table[symbol_count].name = strdup(name);
    symbol_table[symbol_count].offset = next_offset;
    next_offset -= 8; // Assign 8 bytes per variable for simplicity
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
    next_offset = -16;
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
            fprintf(out, "    mov w0, %s\n", node->data.literal.value);
            break;
        case AST_BINOP:
            codegen_expr(node->data.binop.left, out, target);
            fprintf(out, "    str x0, [sp, #-16]!\n"); // Store x0 (64-bit) to keep stack aligned
            codegen_expr(node->data.binop.right, out, target);
            fprintf(out, "    mov w1, w0\n");
            fprintf(out, "    ldr x0, [sp], #16\n");
            if (strcmp(node->data.binop.op, "+") == 0) {
                fprintf(out, "    add w0, w0, w1\n");
            } else if (strcmp(node->data.binop.op, "-") == 0) {
                fprintf(out, "    sub w0, w0, w1\n");
            } else if (strcmp(node->data.binop.op, "*") == 0) {
                fprintf(out, "    mul w0, w0, w1\n");
            } else if (strcmp(node->data.binop.op, "==") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, eq\n");
            } else if (strcmp(node->data.binop.op, "!=") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, ne\n");
            } else if (strcmp(node->data.binop.op, "<") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, lt\n");
            } else if (strcmp(node->data.binop.op, "<=") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, le\n");
            } else if (strcmp(node->data.binop.op, ">") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, gt\n");
            } else if (strcmp(node->data.binop.op, ">=") == 0) {
                fprintf(out, "    cmp w0, w1\n");
                fprintf(out, "    cset w0, ge\n");
            }
            break;
        case AST_IDENT: {
            int offset = get_symbol_offset(node->data.ident.name);
            if (offset != 0) {
                // If the symbol is a local, load its value (assumed 4 bytes for w0, or 8 bytes for x0)
                // For now we use 4-byte loads by default for integers
                fprintf(out, "    ldr w0, [x29, #%d]\n", offset);
            } else {
                fprintf(out, "    // Error: Unknown variable %s\n", node->data.ident.name);
            }
            break;
        }
        case AST_CALL: {
            if (strcmp(node->data.call.name, "Box::new") == 0) {
                // Simplified Box::new: call malloc(8), then store value
                fprintf(out, "    mov x0, #8\n");
                if (target.os == OS_MACOS) fprintf(out, "    bl _malloc\n");
                else fprintf(out, "    bl malloc\n");
                fprintf(out, "    str x0, [sp, #-16]!\n"); // Save pointer
                codegen_expr(node->data.call.args[0], out, target);
                fprintf(out, "    mov w1, w0\n");
                fprintf(out, "    ldr x0, [sp], #16\n"); // Restore pointer
                fprintf(out, "    str w1, [x0]\n"); // Store value into allocated memory
                // x0 still contains the pointer
            } else {
                // Pass arguments in x0-x7
                for (int i = 0; i < node->data.call.arg_count && i < 8; i++) {
                    codegen_expr(node->data.call.args[i], out, target);
                    fprintf(out, "    mov x%d, x0\n", i);
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
                // Address is in x0, load value from it
                fprintf(out, "    ldr w0, [x0]\n");
            } else if (strcmp(node->data.unop.op, "&") == 0) {
                if (node->data.unop.expr->type == AST_IDENT) {
                    int offset = get_symbol_offset(node->data.unop.expr->data.ident.name);
                    if (offset != 0) {
                        fprintf(out, "    sub x0, x29, #%d\n", -offset);
                    } else {
                        fprintf(out, "    // Error: address-of unknown variable\n");
                    }
                } else {
                    fprintf(out, "    // Error: address-of non-identifier not supported yet\n");
                }
            }
            break;
        case AST_MACRO_CALL:
            if (strcmp(node->data.macro_call.name, "println") == 0 || strcmp(node->data.macro_call.name, "print") == 0) {
                // Pass arguments in x0-x7
                for (int i = 0; i < node->data.macro_call.arg_count && i < 8; i++) {
                    codegen_expr(node->data.macro_call.args[i], out, target);
                    fprintf(out, "    mov x%d, x0\n", i);
                }
                if (target.os == OS_MACOS) {
                    fprintf(out, "    bl _printf\n");
                } else {
                    fprintf(out, "    bl printf\n");
                }
                if (strcmp(node->data.macro_call.name, "println") == 0) {
                    // Simplified: just call printf("\n")
                    const char *nl_label = add_string_constant("\\n");
                    if (target.os == OS_MACOS) {
                        fprintf(out, "    adrp x0, %s@PAGE\n", nl_label);
                        fprintf(out, "    add x0, x0, %s@PAGEOFF\n", nl_label);
                        fprintf(out, "    bl _printf\n");
                    } else {
                        fprintf(out, "    adrp x0, %s\n", nl_label);
                        fprintf(out, "    add x0, x0, :lo12:%s\n", nl_label);
                        fprintf(out, "    bl printf\n");
                    }
                }
            }
            break;
        case AST_STRING_LITERAL: {
            const char *label = add_string_constant(node->data.string_literal.value);
            if (target.os == OS_MACOS) {
                fprintf(out, "    adrp x0, %s@PAGE\n", label);
                fprintf(out, "    add x0, x0, %s@PAGEOFF\n", label);
            } else {
                fprintf(out, "    adrp x0, %s\n", label);
                fprintf(out, "    add x0, x0, :lo12:%s\n", label);
            }
            break;
        }
        case AST_STRUCT_INIT:
            // Very simplified struct initialization
            // We just evaluate all field initializers and assume they are laid out in order.
            // Result will be a pointer to a temporary struct on stack.
            fprintf(out, "    sub sp, sp, #32\n"); // Reserve 32 bytes for a small struct
            fprintf(out, "    mov x8, sp\n");
            for (int i = 0; i < node->data.struct_init.field_count; i++) {
                ASTNode *field_init = node->data.struct_init.fields[i];
                codegen_expr(field_init->data.field_init.value, out, target);
                fprintf(out, "    str w0, [x8, #%d]\n", i * 8); // Simplification
            }
            fprintf(out, "    mov x0, x8\n");
            break;
        default:
            fprintf(out, "    // Unsupported ARM64 expression node type %d\n", node->type);
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
                fprintf(out, ".type %s, %%function\n", node->data.func.name);
                fprintf(out, "%s:\n", node->data.func.name);
            }

            // Prologue
            fprintf(out, "    stp x29, x30, [sp, #-48]!\n"); // Reserve space for locals
            fprintf(out, "    mov x29, sp\n");

            // Handle parameters (up to 8 in x0-x7)
            for (int i = 0; i < node->data.func.param_count && i < 8; i++) {
                add_symbol(node->data.func.params[i]->data.param.name);
                int offset = get_symbol_offset(node->data.func.params[i]->data.param.name);
                fprintf(out, "    str x%d, [x29, #%d]\n", i, offset);
            }

            // Body
            codegen_node(node->data.func.body, out, target);

            // Epilogue
            fprintf(out, ".L%s_ret:\n", node->data.func.name);
            fprintf(out, "    ldp x29, x30, [sp], #48\n");
            fprintf(out, "    ret\n\n");
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
            fprintf(out, "    cmp w0, #0\n");
            fprintf(out, "    b.eq .L%d\n", l_else);
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
            fprintf(out, "    cmp w0, #0\n");
            fprintf(out, "    b.eq .L%d\n", l_end);
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
                fprintf(out, "    str w0, [x29, #%d]\n", offset);
            } else {
                add_symbol(node->data.var_decl.name);
            }
            break;

        case AST_MATCH: {
            int l_end = next_label();
            codegen_expr(node->data.match_stmt.expr, out, target);
            // Result of expr is in x0. 
            // In C backend, match works on .tag. In our simplified ARM64, 
            // we assume enums are structs where tag is the first 4-byte field.
            fprintf(out, "    ldr w0, [x0]\n"); // Load tag from struct pointer in x0
            
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                ASTNode *arm = node->data.match_stmt.arms[i];
                int l_next = next_label();
                int l_body = next_label();

                // Save current tag value as we might overwrite w0 in pattern evaluation
                fprintf(out, "    str x0, [sp, #-16]!\n"); 
                
                // Pattern comparison. 
                // Simplified: if pattern is IDENT, we might need its value.
                // For now, let's assume patterns are literals or we treat them as such.
                if (arm->data.match_arm.pattern->type == AST_IDENT && strcmp(arm->data.match_arm.pattern->data.ident.name, "_") == 0) {
                    // Wildcard: always matches
                    fprintf(out, "    ldr x0, [sp], #16\n"); // Restore tag to x0 (though not needed)
                    codegen_node(arm->data.match_arm.body, out, target);
                    fprintf(out, "    b .L%d\n", l_end);
                } else {
                    codegen_expr(arm->data.match_arm.pattern, out, target);
                    fprintf(out, "    mov w1, w0\n");
                    fprintf(out, "    ldr x0, [sp], #16\n"); // Restore tag to x0
                    
                    fprintf(out, "    cmp w0, w1\n");
                    fprintf(out, "    b.eq .Lbody_%d\n", l_body);
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
            // Receiver address in x0.
            // Simplified: we don't have a real type system to know offsets.
            // Assume the field "tag" is at offset 0, and "data" starts at offset 8.
            if (strcmp(node->data.field_access.field_name, "tag") == 0) {
                fprintf(out, "    ldr w0, [x0]\n");
            } else {
                fprintf(out, "    // ARM64: field access for %s assumed at offset 8\n", node->data.field_access.field_name);
                fprintf(out, "    ldr x0, [x0, #8]\n");
            }
            break;

        case AST_STRUCT_DECL:
        case AST_ENUM_DECL:
        case AST_TRAIT:
        case AST_IMPL:
        case AST_MOD:
        case AST_USE:
            fprintf(out, "// ARM64: top-level declaration ignored in asm backend\n");
            break;

        case AST_LITERAL:
        case AST_BINOP:
        case AST_IDENT:
        case AST_CALL:
        case AST_MACRO_CALL:
        case AST_UNOP:
            codegen_expr(node, out, target);
            break;

        default:
            fprintf(out, "    // Unsupported ARM64 node type %d\n", node->type);
            break;
    }
}

void codegen_arm64_generate(ASTNode *node, FILE *out, Target target) {
    if (!node) return;
    
    // Header for ARM64 assembly
    if (node->type == AST_FUNC && strcmp(node->data.func.name, "main") == 0) {
        fprintf(out, "// ARM64 Assembly generated by nbrust\n");
        fprintf(out, ".section .text\n\n");
    }

    codegen_node(node, out, target);

    // After all nodes, emit string constants
    if (string_count > 0) {
        fprintf(out, "\n.section .rodata\n");
        for (int i = 0; i < string_count; i++) {
            fprintf(out, "%s:\n", string_constants[i].label);
            fprintf(out, "    .asciz \"%s\"\n", string_constants[i].value);
        }
        clear_string_constants();
    }
}
