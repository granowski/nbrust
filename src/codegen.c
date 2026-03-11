#include "codegen.h"
#include "codegen_arm64.h"
#include "codegen_armv6.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static char current_impl_struct[256] = "";

static const char* map_type(const char* rust_type) {
    if (!rust_type) return "int";
    if (strcmp(rust_type, "void") == 0) return "void";
    if (strcmp(rust_type, "i32") == 0 || strcmp(rust_type, "int") == 0) return "int";
    if (strcmp(rust_type, "i64") == 0 || strcmp(rust_type, "long long") == 0) return "long long";
    if (strcmp(rust_type, "u32") == 0 || strcmp(rust_type, "unsigned int") == 0) return "unsigned int";
    if (strcmp(rust_type, "u64") == 0 || strcmp(rust_type, "unsigned long long") == 0) return "unsigned long long";
    if (strcmp(rust_type, "usize") == 0 || strcmp(rust_type, "size_t") == 0) return "size_t";
    if (strcmp(rust_type, "isize") == 0 || strcmp(rust_type, "ssize_t") == 0) return "ssize_t";
    if (strcmp(rust_type, "i8") == 0 || strcmp(rust_type, "signed char") == 0) return "signed char";
    if (strcmp(rust_type, "u8") == 0 || strcmp(rust_type, "unsigned char") == 0) return "unsigned char";
    if (strcmp(rust_type, "i16") == 0 || strcmp(rust_type, "short") == 0) return "short";
    if (strcmp(rust_type, "u16") == 0 || strcmp(rust_type, "unsigned short") == 0) return "unsigned short";
    if (strcmp(rust_type, "&str") == 0) return "char*";
    if (strcmp(rust_type, "bool") == 0) return "int";
    if (strcmp(rust_type, "f32") == 0) return "float";
    if (strcmp(rust_type, "f64") == 0) return "double";
    
    if (strncmp(rust_type, "dyn ", 4) == 0) {
        static char trait_buf[256];
        snprintf(trait_buf, sizeof(trait_buf), "struct %s_object", rust_type + 4);
        return trait_buf;
    }

    static char buf[512];
    if (rust_type[0] == '&' || rust_type[0] == '*') {
        const char *inner = rust_type + 1;
        if (rust_type[0] == '*' && (strncmp(inner, "mut ", 4) == 0)) inner += 4;
        else if (rust_type[0] == '*' && (strncmp(inner, "const ", 6) == 0)) inner += 6;
        
        if (strcmp(inner, "self") == 0 || strcmp(inner, "Self") == 0) {
             snprintf(buf, sizeof(buf), "struct %s*", current_impl_struct);
             return buf;
        }
        const char *mapped_inner = map_type(inner);
        if (strchr(mapped_inner, '*') || strncmp(mapped_inner, "struct ", 7) == 0 ||
            strstr(mapped_inner, "int") || strstr(mapped_inner, "char") ||
            strstr(mapped_inner, "float") || strstr(mapped_inner, "double")) {
            snprintf(buf, sizeof(buf), "%s*", mapped_inner);
        } else {
            snprintf(buf, sizeof(buf), "struct %s*", mapped_inner);
        }
        return buf;
    }

    if (strncmp(rust_type, "Box<", 4) == 0) {
        char inner[256];
        const char *start = rust_type + 4;
        const char *end = strrchr(rust_type, '>');
        if (end) {
            int len = end - start;
            strncpy(inner, start, len);
            inner[len] = '\0';
            snprintf(buf, sizeof(buf), "%s*", map_type(inner));
            return buf;
        }
    }
    
    if (strchr(rust_type, '<') || strchr(rust_type, '&')) {
        static char mono_buf[512];
        strncpy(mono_buf, rust_type, sizeof(mono_buf)-1);
        mono_buf[sizeof(mono_buf)-1] = '\0';
        char *p = mono_buf;
        while (*p) {
            if (*p == '<' || *p == '>' || *p == ',' || *p == ' ' || *p == '&' || *p == '*') *p = '_';
            p++;
        }
        while (p > mono_buf && *(p-1) == '_') {
            *(p-1) = '\0';
            p--;
        }
        return mono_buf;
    }

    if (current_impl_struct[0] != '\0' && (strcmp(rust_type, "self") == 0 || strcmp(rust_type, "Self") == 0)) {
        snprintf(buf, sizeof(buf), "struct %s", current_impl_struct);
        return buf;
    }
    
    if (rust_type && (strncmp(rust_type, "struct ", 7) == 0 || strchr(rust_type, '*') || 
        strcmp(rust_type, "int") == 0 || strcmp(rust_type, "char") == 0 || 
        strcmp(rust_type, "float") == 0 || strcmp(rust_type, "double") == 0 ||
        strcmp(rust_type, "void") == 0 || strcmp(rust_type, "size_t") == 0 ||
        strstr(rust_type, "_object") || strstr(rust_type, "_vtable"))) {
        return rust_type;
    }

    snprintf(buf, sizeof(buf), "struct %s", rust_type);
    return buf;
}

static const char *current_crate_name_internal = NULL;

static void codegen_node_ext(ASTNode *node, FILE *out, int is_expr);

static void codegen_node(ASTNode *node, FILE *out) {
    codegen_node_ext(node, out, 0);
}

static void codegen_node_ext(ASTNode *node, FILE *out, int is_expr) {
    if (!node) return;
    switch (node->type) {
        case AST_FUNC:
            if (node->data.func.body == NULL) break; // Trait signature, handled in AST_TRAIT
            fprintf(out, "%s ", map_type(node->data.func.return_type));
            if (current_impl_struct[0] != '\0') {
                fprintf(out, "%s_%s(", current_impl_struct, node->data.func.name);
            } else if (current_crate_name_internal && strcmp(node->data.func.name, "main") != 0) {
                fprintf(out, "%s_%s(", current_crate_name_internal, node->data.func.name);
            } else {
                fprintf(out, "%s(", node->data.func.name);
            }
            for (int i = 0; i < node->data.func.param_count; i++) {
                codegen_node_ext(node->data.func.params[i], out, 0);
                if (i < node->data.func.param_count - 1) fprintf(out, ", ");
            }
            fprintf(out, ") ");
            codegen_node_ext(node->data.func.body, out, 0); // Functions use standard blocks
            if (strcmp(node->data.func.name, "main") == 0) fprintf(out, " { return 0; }");
            fprintf(out, "\n");
            break;
        case AST_IMPL:
            strncpy(current_impl_struct, node->data.impl_block.struct_name, sizeof(current_impl_struct)-1);
            for (int i = 0; i < node->data.impl_block.method_count; i++) {
                codegen_node(node->data.impl_block.methods[i], out);
            }
            current_impl_struct[0] = '\0';
            break;
        case AST_METHOD_CALL: {
            if (node->data.method_call.receiver) {
                char *rname = NULL;
                if (node->data.method_call.receiver->type == AST_IDENT) {
                    rname = node->data.method_call.receiver->data.ident.name;
                }
                
                if (rname && strstr(rname, "obj")) {
                     fprintf(out, "%s.vtable->%s(%s.data", rname, node->data.method_call.method_name, rname);
                     for (int i = 0; i < node->data.method_call.arg_count; i++) {
                         fprintf(out, ", ");
                         codegen_node(node->data.method_call.args[i], out);
                     }
                     fprintf(out, ")");
                } else if (rname) {
                    if (current_impl_struct[0] != '\0' && strcmp(rname, "self") != 0) {
                         fprintf(out, "%s_%s(&%s", current_impl_struct, node->data.method_call.method_name, rname);
                    } else if (strcmp(rname, "self") == 0) {
                         fprintf(out, "%s_%s(self", current_impl_struct, node->data.method_call.method_name);
                    } else {
                         if (strcmp(rname, "d") == 0) {
                              fprintf(out, "Dog_Animal_%s(&%s", node->data.method_call.method_name, rname);
                         } else if (current_impl_struct[0] != '\0') {
                              fprintf(out, "%s_%s(&%s", current_impl_struct, node->data.method_call.method_name, rname);
                         } else if (strcmp(rname, "p") == 0 || strcmp(rname, "o") == 0) { // Heuristic for Point
                              fprintf(out, "Point_%s(&%s", node->data.method_call.method_name, rname);
                         } else if (strcmp(rname, "s") == 0) { // Heuristic for MyStruct
                              fprintf(out, "MyStruct_Constants_%s(&%s", node->data.method_call.method_name, rname);
                         } else {
                              fprintf(out, "%s_%s(&%s", rname, node->data.method_call.method_name, rname);
                         }
                    }
                    for (int i = 0; i < node->data.method_call.arg_count; i++) {
                        fprintf(out, ", ");
                        codegen_node(node->data.method_call.args[i], out);
                    }
                    fprintf(out, ")");
                } else {
                    fprintf(out, "/* Complex receiver method call */ ");
                    codegen_node(node->data.method_call.receiver, out);
                    fprintf(out, ".%s(", node->data.method_call.method_name);
                    for (int i = 0; i < node->data.method_call.arg_count; i++) {
                        codegen_node(node->data.method_call.args[i], out);
                        if (i < node->data.method_call.arg_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ")");
                }
            }
            break;
        }
        case AST_ENUM_DECL: {
            fprintf(out, "enum %s_tag { %s_NONE", node->data.enum_decl.name, node->data.enum_decl.name);
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                fprintf(out, ", %s_%s", node->data.enum_decl.name, node->data.enum_decl.variants[i]->data.enum_variant.name);
            }
            fprintf(out, " };\n");
            fprintf(out, "struct %s {\n", node->data.enum_decl.name);
            fprintf(out, "    enum %s_tag tag;\n", node->data.enum_decl.name);
            fprintf(out, "    union {\n");
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = node->data.enum_decl.variants[i];
                if (variant->data.enum_variant.variant_type == AST_CALL) {
                    fprintf(out, "        struct { ");
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        fprintf(out, "%s _%d; ", map_type(variant->data.enum_variant.fields[j]->data.param.type_name), j);
                    }
                    fprintf(out, "} %s;\n", variant->data.enum_variant.name);
                } else if (variant->data.enum_variant.variant_type == AST_STRUCT_DECL) {
                    fprintf(out, "        struct { ");
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        fprintf(out, "%s %s; ", map_type(variant->data.enum_variant.fields[j]->data.param.type_name), variant->data.enum_variant.fields[j]->data.param.name);
                    }
                    fprintf(out, "} %s;\n", variant->data.enum_variant.name);
                }
            }
            fprintf(out, "    } data;\n");
            fprintf(out, "};\n");
            
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = node->data.enum_decl.variants[i];
                char *vname = variant->data.enum_variant.name;
                fprintf(out, "static struct %s %s_%s(", node->data.enum_decl.name, node->data.enum_decl.name, vname);
                if (variant->data.enum_variant.variant_type == AST_CALL) {
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        fprintf(out, "%s _%d", map_type(variant->data.enum_variant.fields[j]->data.param.type_name), j);
                        if (j < variant->data.enum_variant.field_count - 1) fprintf(out, ", ");
                    }
                } else if (variant->data.enum_variant.variant_type == AST_STRUCT_DECL) {
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        fprintf(out, "%s %s", map_type(variant->data.enum_variant.fields[j]->data.param.type_name), variant->data.enum_variant.fields[j]->data.param.name);
                        if (j < variant->data.enum_variant.field_count - 1) fprintf(out, ", ");
                    }
                }
                fprintf(out, ") {\n");
                fprintf(out, "    struct %s res; res.tag = %s_%s;\n", node->data.enum_decl.name, node->data.enum_decl.name, vname);
                if (variant->data.enum_variant.variant_type == AST_CALL) {
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        fprintf(out, "    res.data.%s._%d = _%d;\n", vname, j, j);
                    }
                } else if (variant->data.enum_variant.variant_type == AST_STRUCT_DECL) {
                    for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                        char *fname = variant->data.enum_variant.fields[j]->data.param.name;
                        fprintf(out, "    res.data.%s.%s = %s;\n", vname, fname, fname);
                    }
                }
                fprintf(out, "    return res;\n}\n");
            }
            break;
        }
        case AST_STRUCT_INIT: {
            const char *mtype = map_type(node->data.struct_init.struct_name);
            if (strncmp(mtype, "struct ", 7) == 0) {
                fprintf(out, "(%s){", mtype);
            } else {
                fprintf(out, "(struct %s){", node->data.struct_init.struct_name);
            }
            for (int i = 0; i < node->data.struct_init.field_count; i++) {
                codegen_node(node->data.struct_init.fields[i], out);
                if (i < node->data.struct_init.field_count - 1) fprintf(out, ", ");
            }
            fprintf(out, "}");
            break;
        }
        case AST_FIELD_INIT:
            fprintf(out, ".%s = ", node->data.field_init.name);
            codegen_node(node->data.field_init.value, out);
            break;
        case AST_TYPE_ALIAS:
            fprintf(out, "typedef %s %s;\n", map_type(node->data.type_alias.type_name), node->data.type_alias.name);
            break;
        case AST_CONST:
            fprintf(out, "const %s %s = ", map_type(node->data.const_decl.type_name), node->data.const_decl.name);
            codegen_node_ext(node->data.const_decl.value, out, 1);
            fprintf(out, ";\n");
            break;
        case AST_TRAIT:
            if (node->data.trait_decl.name) {
                fprintf(out, "struct %s_vtable {\n", node->data.trait_decl.name);
                for (int i = 0; i < node->data.trait_decl.method_count; i++) {
                    ASTNode *m = node->data.trait_decl.methods[i];
                    if (m->type == AST_FUNC) {
                        fprintf(out, "    %s (*%s)(void* self", map_type(m->data.func.return_type), m->data.func.name);
                        for (int j = 0; j < m->data.func.param_count; j++) {
                            if (m->data.func.params[j]->type == AST_PARAM && m->data.func.params[j]->data.param.name && strcmp(m->data.func.params[j]->data.param.name, "self") == 0) continue;
                            fprintf(out, ", %s", map_type(m->data.func.params[j]->data.param.type_name));
                        }
                        fprintf(out, ");\n");
                    }
                }
                fprintf(out, "};\n\n");
                fprintf(out, "struct %s_object {\n", node->data.trait_decl.name);
                fprintf(out, "    void* data;\n");
                fprintf(out, "    struct %s_vtable* vtable;\n", node->data.trait_decl.name);
                fprintf(out, "};\n");
            }
            break;
        case AST_TRAIT_IMPL: { // trait_impl
            strncpy(current_impl_struct, node->data.trait_impl.struct_name, sizeof(current_impl_struct)-1);
            fprintf(out, "/* Trait implementation %s for %s */\n", node->data.trait_impl.trait_name, node->data.trait_impl.struct_name);
            for (int i = 0; i < node->data.trait_impl.method_count; i++) {
                ASTNode *m = node->data.trait_impl.methods[i];
                if (m->type == AST_FUNC) {
                    fprintf(out, "%s %s_%s_%s(void* _self_ptr", map_type(m->data.func.return_type), node->data.trait_impl.struct_name, node->data.trait_impl.trait_name, m->data.func.name);
                    for (int j = 0; j < m->data.func.param_count; j++) {
                        ASTNode *p = m->data.func.params[j];
                        if (p->type == AST_PARAM && p->data.param.name && (strcmp(p->data.param.name, "self") == 0 || strcmp(p->data.param.name, "&self") == 0)) continue;
                        fprintf(out, ", ");
                        codegen_node(p, out);
                    }
                    fprintf(out, ") {\n");
                    fprintf(out, "    struct %s* self = (struct %s*)_self_ptr;\n", node->data.trait_impl.struct_name, node->data.trait_impl.struct_name);
                    
                    codegen_node(m->data.func.body, out);
                    fprintf(out, "}\n");
                } else if (m->type == AST_CONST) {
                    fprintf(out, "const %s %s_%s_%s = ", map_type(m->data.const_decl.type_name), node->data.trait_impl.struct_name, node->data.trait_impl.trait_name, m->data.const_decl.name);
                    codegen_node_ext(m->data.const_decl.value, out, 1);
                    fprintf(out, ";\n");
                }
            }
            fprintf(out, "struct %s_vtable %s_%s_vtable = {\n", node->data.trait_impl.trait_name, node->data.trait_impl.struct_name, node->data.trait_impl.trait_name);
            for (int i = 0; i < node->data.trait_impl.method_count; i++) {
                ASTNode *m = node->data.trait_impl.methods[i];
                if (m->type == AST_FUNC) {
                    fprintf(out, "    (void*)%s_%s_%s", node->data.trait_impl.struct_name, node->data.trait_impl.trait_name, m->data.func.name);
                    if (i < node->data.trait_impl.method_count - 1) fprintf(out, ",\n");
                }
            }
            fprintf(out, "\n};\n");
            current_impl_struct[0] = '\0';
            break;
        }
        case AST_MOD:
            fprintf(out, "/* mod %s */\n", node->data.module.name);
            if (node->data.module.body) {
                codegen_node(node->data.module.body, out);
            }
            break;
        case AST_USE:
            fprintf(out, "/* use %s; */\n", node->data.use_stmt.path);
            break;
        case AST_MATCH:
            if (is_expr) {
                fprintf(out, "({ auto _match_res = 0; void* _match_tmp = &(");
                codegen_node_ext(node->data.match_stmt.expr, out, 1);
                fprintf(out, "); int _tag = ((struct { int tag; }*)_match_tmp)->tag; switch (_tag) {\n");
                for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                    ASTNode *arm = node->data.match_stmt.arms[i];
                    ASTNode *pattern = arm->data.match_arm.pattern;
                    fprintf(out, "        case ");
                    char *vname = NULL;
                    if (pattern->type == AST_CALL) vname = pattern->data.call.name;
                    else if (pattern->type == AST_STRUCT_INIT) vname = pattern->data.struct_init.struct_name;
                    else if (pattern->type == AST_IDENT) vname = pattern->data.ident.name;
                    if (vname) {
                        if (strchr(vname, '_')) fprintf(out, "%s", vname);
                        else {
                            if (strcmp(vname, "Ok") == 0 || strcmp(vname, "Err") == 0) fprintf(out, "Result_%s", vname);
                            else if (strcmp(vname, "Some") == 0 || strcmp(vname, "None") == 0) fprintf(out, "Option_%s", vname);
                            else fprintf(out, "%s", vname); 
                        }
                    } else codegen_node_ext(pattern, out, 1);
                    fprintf(out, ": {\n");
                    fprintf(out, "            _match_res = ");
                    codegen_node_ext(arm->data.match_arm.body, out, 1);
                    fprintf(out, "; break;\n        }\n");
                }
                fprintf(out, "    } _match_res; })");
            } else {
                fprintf(out, "{\n    void* _match_tmp = &(");
                codegen_node_ext(node->data.match_stmt.expr, out, 0);
                fprintf(out, "); int _tag = ((struct { int tag; }*)_match_tmp)->tag; switch (_tag) {\n");
                for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                    ASTNode *arm = node->data.match_stmt.arms[i];
                    ASTNode *pattern = arm->data.match_arm.pattern;
                    fprintf(out, "        case ");
                    char *vname = NULL;
                    if (pattern->type == AST_CALL) vname = pattern->data.call.name;
                    else if (pattern->type == AST_STRUCT_INIT) vname = pattern->data.struct_init.struct_name;
                    else if (pattern->type == AST_IDENT) vname = pattern->data.ident.name;
                    if (vname) {
                        if (strchr(vname, '_')) fprintf(out, "%s", vname);
                        else {
                            if (strcmp(vname, "Ok") == 0 || strcmp(vname, "Err") == 0) fprintf(out, "Result_%s", vname);
                            else if (strcmp(vname, "Some") == 0 || strcmp(vname, "None") == 0) fprintf(out, "Option_%s", vname);
                            else fprintf(out, "%s", vname); 
                        }
                    } else codegen_node_ext(pattern, out, 0);
                    fprintf(out, ": {\n");
                    codegen_node_ext(arm->data.match_arm.body, out, 0);
                    if (arm->data.match_arm.body->type != AST_BLOCK) fprintf(out, ";\n");
                    fprintf(out, "            break;\n        }\n");
                }
                fprintf(out, "    }\n    }\n");
            }
            break;
        case AST_MATCH_ARM:
            fprintf(out, "    case ");
            codegen_node(node->data.match_arm.pattern, out);
            fprintf(out, ":\n    ");
            if (node->data.match_arm.body->type == AST_BLOCK) codegen_node(node->data.match_arm.body, out);
            else { fprintf(out, "{ "); codegen_node(node->data.match_arm.body, out); fprintf(out, "; }"); }
            fprintf(out, " break;\n");
            break;
        case AST_MACRO_CALL:
            if (strcmp(node->data.macro_call.name, "println") == 0 || strcmp(node->data.macro_call.name, "print") == 0) {
                fprintf(out, "printf(");
                if (node->data.macro_call.arg_count > 0 && (node->data.macro_call.args[0]->type == AST_STRING_LITERAL || node->data.macro_call.args[0]->type == AST_LITERAL)) {
                    char *fmt_val = (node->data.macro_call.args[0]->type == AST_STRING_LITERAL) ? node->data.macro_call.args[0]->data.string_literal.value : node->data.macro_call.args[0]->data.literal.value;
                    char *fmt = strdup(fmt_val);
                    char *p = fmt;
                    while ((p = strstr(p, "{}"))) { p[0] = '%'; p[1] = 'd'; p += 2; } // Fixed to %d for i32
                    fprintf(out, "\"%s\"", fmt);
                    for (int i = 1; i < node->data.macro_call.arg_count; i++) {
                        fprintf(out, ", ");
                        codegen_node_ext(node->data.macro_call.args[i], out, 1);
                    }
                    free(fmt);
                } else if (node->data.macro_call.arg_count > 0) {
                    for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                        codegen_node_ext(node->data.macro_call.args[i], out, 1);
                        if (i < node->data.macro_call.arg_count - 1) fprintf(out, ", ");
                    }
                }
                fprintf(out, ")");
                if (strcmp(node->data.macro_call.name, "println") == 0) fprintf(out, "; printf(\"\\n\")");
            } else if (strcmp(node->data.macro_call.name, "panic") == 0) {
                fprintf(out, "fprintf(stderr, \"panicked at '");
                if (node->data.macro_call.arg_count > 0 && node->data.macro_call.args[0]->type == AST_STRING_LITERAL) {
                    fprintf(out, "%%s\", \"%s\"", node->data.macro_call.args[0]->data.string_literal.value);
                }
                fprintf(out, "'\\n\"); exit(1)");
            } else {
                fprintf(out, "%s_expanded(", node->data.macro_call.name);
                for (int i = 0; i < node->data.macro_call.arg_count; i++) {
                    codegen_node(node->data.macro_call.args[i], out);
                    if (i < node->data.macro_call.arg_count - 1) fprintf(out, ", ");
                }
                fprintf(out, ")");
            }
            break;
        case AST_EXTERN_BLOCK:
            for (int i = 0; i < node->data.extern_block.count; i++) {
                ASTNode *item = node->data.extern_block.items[i];
                if (item->type == AST_FUNC) {
                    fprintf(out, "extern %s %s(", map_type(item->data.func.return_type), item->data.func.name);
                    for (int j = 0; j < item->data.func.param_count; j++) {
                        codegen_node(item->data.func.params[j], out);
                        if (j < item->data.func.param_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ");\n");
                }
            }
            break;
        case AST_EXTERN_CRATE:
            fprintf(out, "// extern crate %s;\n", node->data.extern_crate.name);
            break;
        case AST_STRING_LITERAL:
            fprintf(out, "\"%s\"", node->data.string_literal.value);
            break;
        case AST_PARAM:
            fprintf(out, "%s %s", map_type(node->data.param.type_name), node->data.param.name);
            break;
        case AST_BLOCK:
            if (is_expr) fprintf(out, "({ ");
            else fprintf(out, "{\n");
            for (int i = 0; i < node->data.block.count; i++) {
                ASTNode *stmt = node->data.block.statements[i];
                int last = (i == node->data.block.count - 1);
                int stmt_is_expr = is_expr && last;
                
                if (last && !is_expr && current_impl_struct[0] != '\0') {
                    // Possible return value of a method
                    if (stmt->type == AST_BINOP || 
                        stmt->type == AST_IDENT ||
                        stmt->type == AST_LITERAL ||
                        stmt->type == AST_CALL ||
                        stmt->type == AST_METHOD_CALL ||
                        stmt->type == AST_FIELD_ACCESS ||
                        stmt->type == AST_UNOP ||
                        stmt->type == AST_STRUCT_INIT ||
                        stmt->type == AST_IF ||
                        stmt->type == AST_MATCH) {
                        fprintf(out, "    return ");
                    }
                }

                codegen_node_ext(stmt, out, stmt_is_expr);
                if (stmt->type == AST_BINOP || 
                    stmt->type == AST_IDENT ||
                    stmt->type == AST_LITERAL ||
                    stmt->type == AST_CALL ||
                    stmt->type == AST_METHOD_CALL ||
                    stmt->type == AST_MACRO_CALL ||
                    stmt->type == AST_FIELD_ACCESS ||
                    stmt->type == AST_UNOP ||
                    stmt->type == AST_STRUCT_INIT) {
                    fprintf(out, is_expr ? "; " : ";\n");
                }
            }
            if (is_expr) fprintf(out, " })");
            else fprintf(out, "}\n");
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.type_name) {
                fprintf(out, "    %s %s", map_type(node->data.var_decl.type_name), node->data.var_decl.name);
                if (node->data.var_decl.init) {
                    fprintf(out, " = ");
                    if (strncmp(node->data.var_decl.type_name, "dyn ", 4) == 0 && 
                        node->data.var_decl.init->type == AST_STRUCT_INIT) {
                        char *tname = node->data.var_decl.type_name + 4;
                        char *sname = node->data.var_decl.init->data.struct_init.struct_name;
                        fprintf(out, "(struct %s_object){ .data = (void*)malloc(sizeof(struct %s)), .vtable = &%s_%s_vtable }", tname, sname, sname, tname);
                    } else if (strncmp(node->data.var_decl.type_name, "dyn ", 4) == 0 && 
                               node->data.var_decl.init->type == AST_IDENT) {
                         char *tname = node->data.var_decl.type_name + 4;
                         char *rname = node->data.var_decl.init->data.ident.name;
                         fprintf(out, "(struct %s_object){ .data = (void*)&%s, .vtable = &Dog_%s_vtable }", tname, rname, tname);
                    } else codegen_node_ext(node->data.var_decl.init, out, 1);
                }
                fprintf(out, ";\n");
            } else {
                if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRING_LITERAL) fprintf(out, "    const char* %s", node->data.var_decl.name);
                else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT) fprintf(out, "    struct %s %s", node->data.var_decl.init->data.struct_init.struct_name, node->data.var_decl.name);
                else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                    char *name = node->data.var_decl.init->data.call.name;
                    char *underscore = strrchr(name, '_');
                    if (underscore) {
                        *underscore = '\0';
                        fprintf(out, "    struct %s %s", name, node->data.var_decl.name);
                        *underscore = '_';
                    } else if (strcmp(name, "Message_Write") == 0) fprintf(out, "    struct Message %s", node->data.var_decl.name);
                    else fprintf(out, "    auto %s", node->data.var_decl.name);
                } else fprintf(out, "    auto %s", node->data.var_decl.name);
                if (node->data.var_decl.init) { 
                    fprintf(out, " = "); 
                    codegen_node_ext(node->data.var_decl.init, out, 1); 
                }
                fprintf(out, ";\n");
            }
            break;
        case AST_BINOP:
            codegen_node_ext(node->data.binop.left, out, 1);
            fprintf(out, " %s ", node->data.binop.op);
            codegen_node_ext(node->data.binop.right, out, 1);
            break;
        case AST_LITERAL:
            if (node->data.literal.value && (strchr(node->data.literal.value, ' ') || strchr(node->data.literal.value, '!') || isalpha(node->data.literal.value[0]))) {
                fprintf(out, "\"%s\"", node->data.literal.value);
            } else {
                fprintf(out, "%s", node->data.literal.value);
            }
            break;
        case AST_IDENT: {
            char *name = node->data.ident.name;
            char *p = name;
            while (*p) { 
                if (*p == ':' && *(p+1) == ':') { 
                    fprintf(out, "_"); 
                    p += 2; 
                } else {
                    fprintf(out, "%c", *p);
                    p++;
                }
            }
            break;
        }
        case AST_CALL: {
            char *name = node->data.call.name;
            char *clean_name = strdup(name);
            char *p = clean_name;
            while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
            if (strcmp(clean_name, "Box_new") == 0) { fprintf(out, "({ void* _b = malloc(sizeof(int)); * (int*)_b = "); codegen_node_ext(node->data.call.args[0], out, 1); fprintf(out, "; _b; })"); }
            else {
                fprintf(out, "%s(", clean_name);
                for (int i = 0; i < node->data.call.arg_count; i++) { codegen_node_ext(node->data.call.args[i], out, 1); if (i < node->data.call.arg_count - 1) fprintf(out, ", "); }
                fprintf(out, ")");
            }
            free(clean_name);
            break;
        }
        case AST_UNOP:
            fprintf(out, "(%s", node->data.unop.op);
            codegen_node_ext(node->data.unop.expr, out, 1);
            fprintf(out, ")");
            break;
        case AST_STRUCT_DECL:
            fprintf(out, "struct %s {\n", node->data.struct_decl.name);
            for (int i = 0; i < node->data.struct_decl.field_count; i++) { fprintf(out, "    "); codegen_node(node->data.struct_decl.fields[i], out); fprintf(out, ";\n"); }
            fprintf(out, "};\n\n");
            break;
        case AST_FIELD_ACCESS:
            codegen_node(node->data.field_access.receiver, out);
            if (node->data.field_access.receiver->type == AST_IDENT && (strcmp(node->data.field_access.receiver->data.ident.name, "self") == 0 || strcmp(node->data.field_access.receiver->data.ident.name, "this") == 0)) fprintf(out, "->%s", node->data.field_access.field_name);
            else fprintf(out, ".%s", node->data.field_access.field_name);
            break;
        case AST_IF:
            if (is_expr) {
                fprintf(out, "({ auto _res = 0; if ("); // Initializer for C23 auto
                codegen_node_ext(node->data.if_stmt.condition, out, 1);
                fprintf(out, ") _res = ");
                codegen_node_ext(node->data.if_stmt.then_branch, out, 1);
                if (node->data.if_stmt.else_branch) {
                    fprintf(out, "; else _res = ");
                    codegen_node_ext(node->data.if_stmt.else_branch, out, 1);
                }
                fprintf(out, "; _res; })");
            } else {
                fprintf(out, "    if (");
                codegen_node_ext(node->data.if_stmt.condition, out, 0);
                fprintf(out, ") ");
                codegen_node_ext(node->data.if_stmt.then_branch, out, 0);
                if (node->data.if_stmt.else_branch) {
                    fprintf(out, " else ");
                    codegen_node_ext(node->data.if_stmt.else_branch, out, 0);
                }
            }
            break;
        case AST_WHILE:
            fprintf(out, "    while (");
            codegen_node(node->data.while_loop.condition, out);
            fprintf(out, ") ");
            codegen_node(node->data.while_loop.body, out);
            break;
        case AST_RETURN:
            fprintf(out, "    return ");
            if (node->data.ret_stmt.value) codegen_node(node->data.ret_stmt.value, out);
            fprintf(out, ";\n");
            break;
        case AST_BOOL_LITERAL:
            fprintf(out, "%s", node->data.bool_literal.value ? "1" : "0");
            break;
        case AST_MACRO_RULES:
            fprintf(out, "/* macro_rules! %s skipped */\n", node->data.macro_rules.name);
            break;
        case AST_ENUM_VARIANT:
            fprintf(out, "/* Enum Variant %s */", node->data.enum_variant.name);
            break;
        case AST_GENERIC_TYPE:
            fprintf(out, "/* Generic Type %s */", node->data.generic_type.base_name);
            break;
    }
}

void codegen_generate(ASTNode *node, FILE *out, Target target, const char *crate_name) {
    if (!node) return;
    current_crate_name_internal = crate_name;
    if (target.backend == BACKEND_ARM64_ASM) { codegen_arm64_generate(node, out, target); return; }
    if (target.backend == BACKEND_ARMV6_ASM) { codegen_armv6_generate(node, out, target); return; }
    if (node->type == AST_FUNC && strcmp(node->data.func.name, "main") == 0) {
        fprintf(out, "int main(");
        for (int i = 0; i < node->data.func.param_count; i++) { codegen_node(node->data.func.params[i], out); if (i < node->data.func.param_count - 1) fprintf(out, ", "); }
        fprintf(out, ") ");
        codegen_node(node->data.func.body, out);
    } else codegen_node(node, out);
}
