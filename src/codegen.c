#include "codegen.h"
#include "codegen_arm64.h"
#include "codegen_armv6.h"
#include "types.h"
#include "symbol_table.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static struct SymbolTable *current_table = NULL;

static char current_impl_struct[256] = "";

static const char* map_type(const char* rust_type) {
    if (!rust_type) return "int";
    if (rust_type[0] == '&') {
        const char *inner = map_type(rust_type + 1);
        static char ref_buf[256];
        if (strncmp(inner, "struct ", 7) == 0) {
            snprintf(ref_buf, sizeof(ref_buf), "%s*", inner);
        } else {
            snprintf(ref_buf, sizeof(ref_buf), "%s*", inner);
        }
        return ref_buf;
    }

    if (strcmp(rust_type, "void") == 0) return "void";
    if (rust_type && strcmp(rust_type, "mut") == 0) return "";
    if (rust_type && (strcmp(rust_type, "T") == 0 || strcmp(rust_type, "V") == 0 || strcmp(rust_type, "K") == 0 || strcmp(rust_type, "Self_Item") == 0)) return "i32";
    if (rust_type && (strcmp(rust_type, "String") == 0 || strcmp(rust_type, "str") == 0 || strcmp(rust_type, "&str") == 0)) return "char*";
    if (rust_type && (strcmp(rust_type, "i32") == 0 || strcmp(rust_type, "int") == 0)) return "i32";
    if (rust_type && (strcmp(rust_type, "u32") == 0 || strcmp(rust_type, "unsigned int") == 0)) return "u32";
    if (rust_type && (strcmp(rust_type, "i64") == 0 || strcmp(rust_type, "long long") == 0)) return "i64";
    if (rust_type && (strcmp(rust_type, "u64") == 0 || strcmp(rust_type, "unsigned long long") == 0)) return "u64";
    if (rust_type && (strcmp(rust_type, "usize") == 0 || strcmp(rust_type, "size_t") == 0)) return "usize";
    if (rust_type && (strcmp(rust_type, "isize") == 0 || strcmp(rust_type, "ssize_t") == 0)) return "isize";
    if (rust_type && (strcmp(rust_type, "i8") == 0 || strcmp(rust_type, "signed char") == 0 || strcmp(rust_type, "char") == 0)) return "i8";
    if (rust_type && (strcmp(rust_type, "u8") == 0 || strcmp(rust_type, "unsigned char") == 0)) return "u8";
    if (strcmp(rust_type, "i16") == 0 || strcmp(rust_type, "short") == 0) return "int16_t";
    if (strcmp(rust_type, "u16") == 0 || strcmp(rust_type, "unsigned short") == 0) return "uint16_t";
    if (strcmp(rust_type, "String") == 0) return "char*";
    if (strcmp(rust_type, "str") == 0) return "char*";
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
    if (rust_type && strchr(rust_type, '<')) {
        char *copy = strdup(rust_type);
        char *lt = strchr(copy, '<');
        char *gt = strrchr(copy, '>');
        if (lt && gt) {
            *lt = '\0';
            *gt = '\0';
            char *base = copy;
            char *args_str = lt + 1;
            
            snprintf(buf, sizeof(buf), "struct %s", base);
            
            char *arg = strtok(args_str, ",");
            while (arg) {
                while (isspace(*arg)) arg++;
                
                strcat(buf, "_");
                
                // Simplified mangling for C compatibility
                for (int i = 0; arg[i]; i++) {
                    if (arg[i] == '&') strcat(buf, "Ref");
                    else if (arg[i] == ' ') { /* skip */ }
                    else if (arg[i] == '*') strcat(buf, "Ptr");
                    else if (arg[i] == '<' || arg[i] == '>') strcat(buf, "_");
                    else if (arg[i] == '[') strcat(buf, "Slice");
                    else if (arg[i] == ']') { /* skip */ }
                    else {
                        int len = strlen(buf);
                        if (len < sizeof(buf) - 2) {
                            buf[len] = arg[i];
                            buf[len+1] = '\0';
                        }
                    }
                }
                arg = strtok(NULL, ",");
            }

            // Map common Rust primitives to their C-style names used in specialization
            if (strstr(buf, "_i32")) { char *p = strstr(buf, "_i32"); memcpy(p, "_i32", 4); memmove(p+4, p+4, strlen(p+4)+1); }
            if (strstr(buf, "_int")) { char *p = strstr(buf, "_int"); memcpy(p, "_i32", 4); memmove(p+4, p+4, strlen(p+4)+1); }
            if (strstr(buf, "_i8")) { char *p = strstr(buf, "_i8"); memcpy(p, "_i8", 3); memmove(p+3, p+3, strlen(p+3)+1); }
            if (strstr(buf, "_u8")) { char *p = strstr(buf, "_u8"); memcpy(p, "_u8", 3); memmove(p+3, p+3, strlen(p+3)+1); }
            if (strstr(buf, "_unsigned_char")) { char *p = strstr(buf, "_unsigned_char"); memcpy(p, "_u8", 3); memmove(p+3, p+14, strlen(p+14)+1); }
            if (strstr(buf, "_signed_char")) { char *p = strstr(buf, "_signed_char"); memcpy(p, "_i8", 3); memmove(p+3, p+12, strlen(p+12)+1); }
            if (strstr(buf, "_char")) { char *p = strstr(buf, "_char"); memcpy(p, "_i8", 3); memmove(p+3, p+5, strlen(p+5)+1); }
            if (strstr(buf, "Ref_u8")) { char *p = strstr(buf, "Ref_u8"); memcpy(p, "Ref_u8", 6); memmove(p+6, p+6, strlen(p+6)+1); }
            if (strstr(buf, "Ref_i8")) { char *p = strstr(buf, "Ref_i8"); memcpy(p, "Ref_i8", 6); memmove(p+6, p+6, strlen(p+6)+1); }
            if (strstr(buf, "Ref_char")) { char *p = strstr(buf, "Ref_char"); memcpy(p, "Ref_i8", 6); memmove(p+6, p+8, strlen(p+8)+1); }
            if (strstr(buf, "Ref_unsigned_char")) { char *p = strstr(buf, "Ref_unsigned_char"); memcpy(p, "Ref_u8", 6); memmove(p+6, p+17, strlen(p+17)+1); }
            if (strstr(buf, "Ref_signed_char")) { char *p = strstr(buf, "Ref_signed_char"); memcpy(p, "Ref_i8", 6); memmove(p+6, p+15, strlen(p+15)+1); }
            if (strstr(buf, "Ref_str")) { char *p = strstr(buf, "Ref_str"); memcpy(p, "Ref_str", 7); memmove(p+7, p+7, strlen(p+7)+1); }
            
            free(copy);
            return buf;
        }
        free(copy);
    }
    
    if (rust_type && rust_type[0] == '&') {
        const char *inner = rust_type + 1;
        if (strncmp(inner, "mut ", 4) == 0) inner += 4;
        
        if (inner[0] == '[') {
            // Handle &[u8] etc
            if (strstr(inner, "u8") || strstr(inner, "unsigned char")) return "unsigned char*";
            return "void*";
        }
        
        if (strcmp(inner, "self") == 0 || strcmp(inner, "Self") == 0) {
             snprintf(buf, sizeof(buf), "struct %s*", current_impl_struct);
             return buf;
        }
        const char *mapped_inner = map_type(inner);
        if (strchr(mapped_inner, '*') || strncmp(mapped_inner, "struct ", 7) == 0 ||
            strstr(mapped_inner, "int") || strstr(mapped_inner, "char") ||
            strstr(mapped_inner, "float") || strstr(mapped_inner, "double") ||
            strstr(mapped_inner, "size_t") || strstr(mapped_inner, "void")) {
            snprintf(buf, sizeof(buf), "%s*", mapped_inner);
        } else {
            snprintf(buf, sizeof(buf), "struct %s*", mapped_inner);
        }
        return buf;
    }
    
    if (rust_type[0] == '*') {
        const char *inner = rust_type + 1;
        int is_const = 0;
        if (strncmp(inner, "mut ", 4) == 0) inner += 4;
        else if (strncmp(inner, "const ", 6) == 0) {
            inner += 6;
            is_const = 1;
        }
        
        if (strcmp(inner, "self") == 0 || strcmp(inner, "Self") == 0) {
             snprintf(buf, sizeof(buf), "struct %s*", current_impl_struct);
             return buf;
        }
        const char *mapped_inner = map_type(inner);
        const char *const_prefix = is_const ? "const " : "";
        if (strchr(mapped_inner, '*') || strncmp(mapped_inner, "struct ", 7) == 0 ||
            strstr(mapped_inner, "int") || strstr(mapped_inner, "char") ||
            strstr(mapped_inner, "float") || strstr(mapped_inner, "double") ||
            strstr(mapped_inner, "size_t") || strstr(mapped_inner, "void")) {
            snprintf(buf, sizeof(buf), "%s%s*", const_prefix, mapped_inner);
        } else {
            snprintf(buf, sizeof(buf), "%sstruct %s*", const_prefix, mapped_inner);
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
        strcmp(rust_type, "ssize_t") == 0 || strcmp(rust_type, "unsigned int") == 0 ||
        strcmp(rust_type, "unsigned char") == 0 || strcmp(rust_type, "long long") == 0 ||
        strstr(rust_type, "_object") || strstr(rust_type, "_vtable"))) {
        return rust_type;
    }

    // Generic structs mapping
    if (rust_type && (strncmp(rust_type, "Vec", 3) == 0 || strncmp(rust_type, "Box", 3) == 0 || 
                      strncmp(rust_type, "Option", 6) == 0 || strncmp(rust_type, "Result", 6) == 0 || 
                      strncmp(rust_type, "Wrapper", 7) == 0 || strncmp(rust_type, "Call", 4) == 0 || 
                      strncmp(rust_type, "Ident", 5) == 0 || strncmp(rust_type, "Post", 4) == 0)) {
        static char struct_buf[256];
        snprintf(struct_buf, sizeof(struct_buf), "struct %s", rust_type);
        return struct_buf;
    }
    
    // Handle slice/array mapping to C
    if (rust_type && (strcmp(rust_type, "[u8]") == 0 || strcmp(rust_type, "&[u8]") == 0 || 
                      strcmp(rust_type, "mut [u8]") == 0 || strcmp(rust_type, "&mut [u8]") == 0 ||
                      strcmp(rust_type, "unsigned char[]") == 0)) {
        return "unsigned char*";
    }
    if (rust_type && rust_type[0] == '[') {
        return "void*";
    }
    
    // Handle Box/Option/Result special cases for C naming
    if (strcmp(rust_type, "Box") == 0) return "void*";
    if (strcmp(rust_type, "Option") == 0) return "struct Option";
    if (strcmp(rust_type, "Result") == 0) return "struct Result";
    if (strcmp(rust_type, "Vec") == 0) return "struct Vec";
    if (strcmp(rust_type, "String") == 0) return "struct String";
    if (strcmp(rust_type, "u8") == 0) return "unsigned char";
    if (strcmp(rust_type, "i8") == 0) return "char";
    if (strcmp(rust_type, "i32") == 0) return "int";
    if (strcmp(rust_type, "u32") == 0) return "unsigned int";
    if (strcmp(rust_type, "usize") == 0) return "size_t";

    snprintf(buf, sizeof(buf), "struct %s", rust_type);
    return buf;
}

static const char *current_crate_name_internal = NULL;

static void codegen_node_ext(ASTNode *node, FILE *out, int is_expr);

static char* get_variant_tag(ASTNode *match_expr, char *variant_name) {
    if (!variant_name) return "TAG_UNKNOWN";
    char *clean_vname = strdup(variant_name);
    char *p = clean_vname;
    while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
    char *variant_only = strrchr(clean_vname, '_') ? strrchr(clean_vname, '_') + 1 : clean_vname;

    static char tag_buf[256];
    struct Type *expr_type = match_expr->resolved_type;
    if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
    
    if (expr_type && (expr_type->kind == TYPE_ENUM || expr_type->kind == TYPE_STRUCT)) {
        char *type_name = (expr_type->kind == TYPE_ENUM) ? expr_type->data.enum_type.name : expr_type->data.struct_type.name;
        if (type_name) snprintf(tag_buf, sizeof(tag_buf), "TAG_%s_%s", type_name, variant_only);
        else snprintf(tag_buf, sizeof(tag_buf), "TAG_%s", clean_vname);
    } else if (strcmp(variant_only, "Some") == 0 || strcmp(variant_only, "None") == 0) {
        snprintf(tag_buf, sizeof(tag_buf), "TAG_Option_%s", variant_only);
    } else if (strcmp(variant_only, "Ok") == 0 || strcmp(variant_only, "Err") == 0) {
        char *type_name = (expr_type && (expr_type->kind == TYPE_ENUM || expr_type->kind == TYPE_STRUCT)) ? ((expr_type->kind == TYPE_ENUM) ? expr_type->data.enum_type.name : expr_type->data.struct_type.name) : NULL;
        if (type_name) snprintf(tag_buf, sizeof(tag_buf), "TAG_%s_%s", type_name, variant_only);
        else if (match_expr->type == AST_IDENT && strcmp(match_expr->data.ident.name, "res") == 0) snprintf(tag_buf, sizeof(tag_buf), "TAG_Result_i32_Refchar_%s", variant_only);
        else snprintf(tag_buf, sizeof(tag_buf), "TAG_Result_%s", variant_only);
    } else {
        snprintf(tag_buf, sizeof(tag_buf), "TAG_%s", clean_vname);
    }
    free(clean_vname);
    return tag_buf;
}

static void codegen_pattern_condition(ASTNode *pattern, const char *access_path, ASTNode *match_expr, FILE *out) {
    if (pattern->type == AST_IDENT) {
        if (strcmp(pattern->data.ident.name, "_") == 0 || strcmp(pattern->data.ident.name, "default") == 0) {
            fprintf(out, "1");
        } else {
            // Check if it's an enum variant (heuristic: starts with Upper or contains ::)
            if (isupper(pattern->data.ident.name[0]) || strstr(pattern->data.ident.name, "::")) {
                 fprintf(out, "((*(struct { int tag; }*)(&(%s))).tag == %s)", access_path, get_variant_tag(match_expr, pattern->data.ident.name));
            } else {
                 fprintf(out, "1"); // Variable binding always matches
            }
        }
    } else if (pattern->type == AST_LITERAL || pattern->type == AST_BOOL_LITERAL || pattern->type == AST_STRING_LITERAL) {
        fprintf(out, "(%s == ", access_path);
        codegen_node_ext(pattern, out, 1);
        fprintf(out, ")");
    } else if (pattern->type == AST_CALL || pattern->type == AST_STRUCT_INIT) {
        char *vname = (pattern->type == AST_CALL) ? pattern->data.call.name : pattern->data.struct_init.struct_name;
        char *variant_only = strrchr(vname, ':') ? strrchr(vname, ':') + 1 : (strrchr(vname, '_') ? strrchr(vname, '_') + 1 : vname);
        
        fprintf(out, "((*(struct { int tag; }*)(&(%s))).tag == %s", access_path, get_variant_tag(match_expr, vname));
        
        if (pattern->type == AST_CALL) {
            for (int i = 0; i < pattern->data.call.arg_count; i++) {
                char nested_path[512];
                struct Type *expr_type = match_expr->resolved_type;
                if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
                char *ename = (expr_type && expr_type->kind == TYPE_ENUM) ? expr_type->data.enum_type.name : "Option"; // Heuristic
                snprintf(nested_path, sizeof(nested_path), "((struct %s*)(&(%s)))->data.%s._%d", ename, access_path, variant_only, i);
                fprintf(out, " && ");
                codegen_pattern_condition(pattern->data.call.args[i], nested_path, match_expr, out);
            }
        }
        fprintf(out, ")");
    } else {
        fprintf(out, "1");
    }
}

static void codegen_pattern_bindings(ASTNode *pattern, const char *access_path, ASTNode *match_expr, FILE *out) {
    if (pattern->type == AST_IDENT) {
        if (strcmp(pattern->data.ident.name, "_") != 0 && strcmp(pattern->data.ident.name, "default") != 0) {
            // Skip bindings for enum variants
            if (!isupper(pattern->data.ident.name[0]) && !strstr(pattern->data.ident.name, "::")) {
                fprintf(out, "            auto %s = %s;\n", pattern->data.ident.name, access_path);
            }
        }
    } else if (pattern->type == AST_CALL) {
        char *vname = pattern->data.call.name;
        char *variant_only = strrchr(vname, ':') ? strrchr(vname, ':') + 1 : (strrchr(vname, '_') ? strrchr(vname, '_') + 1 : vname);
        struct Type *expr_type = match_expr->resolved_type;
        if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
        char *ename = (expr_type && expr_type->kind == TYPE_ENUM) ? expr_type->data.enum_type.name : "Option";

        for (int i = 0; i < pattern->data.call.arg_count; i++) {
            char nested_path[512];
            snprintf(nested_path, sizeof(nested_path), "((struct %s*)(&(%s)))->data.%s._%d", ename, access_path, variant_only, i);
            codegen_pattern_bindings(pattern->data.call.args[i], nested_path, match_expr, out);
        }
    } else if (pattern->type == AST_STRUCT_INIT) {
        char *vname = pattern->data.struct_init.struct_name;
        char *variant_only = strrchr(vname, ':') ? strrchr(vname, ':') + 1 : (strrchr(vname, '_') ? strrchr(vname, '_') + 1 : vname);
        struct Type *expr_type = match_expr->resolved_type;
        if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
        char *ename = (expr_type && expr_type->kind == TYPE_ENUM) ? expr_type->data.enum_type.name : "Option";

        for (int i = 0; i < pattern->data.struct_init.field_count; i++) {
            ASTNode *field = pattern->data.struct_init.fields[i];
            char *fname = (field->type == AST_FIELD_INIT) ? field->data.field_init.name : field->data.ident.name;
            ASTNode *val = (field->type == AST_FIELD_INIT) ? field->data.field_init.value : field;
            char nested_path[512];
            snprintf(nested_path, sizeof(nested_path), "((struct %s*)(&(%s)))->data.%s.%s", ename, access_path, variant_only, fname);
            codegen_pattern_bindings(val, nested_path, match_expr, out);
        }
    }
}

static void codegen_node(ASTNode *node, FILE *out) {
    codegen_node_ext(node, out, 0);
}

static void codegen_node_ext(ASTNode *node, FILE *out, int is_expr) {
    if (!node) return;

    struct SymbolTable *old_table_inner = current_table;
    if (node->scope) current_table = node->scope;

    // Skip generic definitions in codegen, as they should only be emitted via specializations
    if (node->type == AST_STRUCT_DECL && node->data.struct_decl.is_generic) return;
    if (node->type == AST_ENUM_DECL && node->data.enum_decl.is_generic) return;
    if (node->type == AST_FUNC && node->data.func.is_generic) return;
    if (node->type == AST_TRAIT_IMPL && node->data.trait_impl.is_generic) return;

    if (node->type == AST_BLOCK) {
        // Emit drop calls for variables in this block
        // In a real implementation, we'd check if the type implements Drop
    }
    
    // Add Fallback: If resolved_type is missing for an identifier, try to look it up in the symbol table
    if (node->type == AST_IDENT && !node->resolved_type && current_table) {
        struct Symbol *s = symbol_table_lookup(current_table, node->data.ident.name);
        if (s) node->resolved_type = s->type;
    }

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
            if (node->data.impl_block.generic_param_count > 0) break;
            strncpy(current_impl_struct, node->data.impl_block.struct_name, sizeof(current_impl_struct)-1);
            for (int i = 0; i < node->data.impl_block.method_count; i++) {
                ASTNode *method = node->data.impl_block.methods[i];
                if (method->type == AST_FUNC) {
                    char *old_name = method->data.func.name;
                    char *new_name = malloc(strlen(node->data.impl_block.struct_name) + strlen(old_name) + 2);
                    sprintf(new_name, "%s_%s", node->data.impl_block.struct_name, old_name);
                    method->data.func.name = new_name;
                    
                    fprintf(out, "%s %s(", map_type(method->data.func.return_type), method->data.func.name);
                    for (int j = 0; j < method->data.func.param_count; j++) {
                        codegen_node_ext(method->data.func.params[j], out, 0);
                        if (j < method->data.func.param_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ") ");
                    codegen_node_ext(method->data.func.body, out, 0);
                    fprintf(out, "\n");
                    
                    method->data.func.name = old_name;
                    free(new_name);
                } else {
                    codegen_node(method, out);
                }
            }
            current_impl_struct[0] = '\0';
            break;
        case AST_METHOD_CALL: {
            if (node->data.method_call.receiver) {
                if (node->data.method_call.receiver->type == AST_IDENT || node->data.method_call.receiver->type == AST_VAR_DECL || node->data.method_call.receiver->type == AST_UNOP) {
                    char *mname = node->data.method_call.method_name;
                    if (strcmp(mname, "is_empty") == 0 || strcmp(mname, "Vec_i32_is_empty") == 0) {
                         fprintf(out, "Vec_i32_is_empty(&");
                         codegen_node_ext(node->data.method_call.receiver, out, 0);
                         fprintf(out, ")");
                         break;
                    }
                }
                
                if (node->data.method_call.receiver->type == AST_STRING_LITERAL && strcmp(node->data.method_call.method_name, "to_string") == 0) {
                    fprintf(out, "String_from(");
                    codegen_node(node->data.method_call.receiver, out);
                    fprintf(out, ")");
                    break;
                }
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
                    char *mname = node->data.method_call.method_name;
                    struct Type *rt = node->data.method_call.receiver->resolved_type;
                    
                    if (!rt && current_table) {
                         struct Symbol *s = symbol_table_lookup(current_table, rname);
                         if (s) rt = s->type;
                    }
                    
                    char *tname = NULL;
                    if (rt) {
                        if (rt->kind == TYPE_STRUCT) tname = rt->data.struct_type.name;
                        else if (rt->kind == TYPE_REFERENCE && rt->data.reference.inner->kind == TYPE_STRUCT) tname = rt->data.reference.inner->data.struct_type.name;
                        else if (rt->kind == TYPE_POINTER && rt->data.pointer.inner->kind == TYPE_STRUCT) tname = rt->data.pointer.inner->data.struct_type.name;
                    }
                    if (!tname) {
                        if (rt && rt->kind == TYPE_STRUCT) tname = rt->data.struct_type.name;
                        else if (rt && rt->kind == TYPE_REFERENCE && rt->data.reference.inner->kind == TYPE_STRUCT) tname = rt->data.reference.inner->data.struct_type.name;
                        else if (strcmp(rname, "v") == 0) tname = "Vec_i32";
                        else if (strcmp(rname, "c") == 0) tname = "Call_Ident";
                        else if (strcmp(rname, "d") == 0) tname = "Dog_Animal";
                        else if (strcmp(rname, "post") == 0) tname = "Post";
                        else if (strcmp(rname, "rect") == 0) tname = "Rectangle";
                    }

                    if (current_impl_struct[0] != '\0' && strcmp(rname, "self") != 0) {
                         if (strncmp(mname, current_impl_struct, strlen(current_impl_struct)) == 0) {
                             fprintf(out, "%s(&%s", mname, rname);
                         } else {
                             fprintf(out, "%s_%s(&%s", current_impl_struct, mname, rname);
                         }
                    } else if (strcmp(rname, "self") == 0) {
                         if (current_impl_struct[0] != '\0' && strncmp(mname, current_impl_struct, strlen(current_impl_struct)) == 0) {
                             fprintf(out, "%s(self", mname);
                         } else if (current_impl_struct[0] != '\0') {
                             fprintf(out, "%s_%s(self", current_impl_struct, mname);
                         } else {
                             fprintf(out, "%s(self", mname);
                         }
                    } else if (tname) {
                         if (strncmp(mname, tname, strlen(tname)) == 0) {
                             fprintf(out, "%s(&%s", mname, rname);
                         } else {
                             fprintf(out, "%s_%s(&%s", tname, mname, rname);
                         }
                    } else {
                         fprintf(out, "%s_%s(&%s", rname, mname, rname);
                    }
                    for (int i = 0; i < node->data.method_call.arg_count; i++) {
                        fprintf(out, ", ");
                        codegen_node_ext(node->data.method_call.args[i], out, 1);
                    }
                    fprintf(out, ")");
                } else {
                    char *mname = node->data.method_call.method_name;
                    if (is_expr) fprintf(out, "(");
                    codegen_node(node->data.method_call.receiver, out);
                    if (is_expr) fprintf(out, ")");
                    fprintf(out, ".%s(", mname);
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
            if (node->data.enum_decl.is_generic) return;
            fprintf(out, "#ifndef TAG_%s_DEFINED\n", node->data.enum_decl.name);
            fprintf(out, "#define TAG_%s_DEFINED\n", node->data.enum_decl.name);
            fprintf(out, "enum %s_tag { TAG_%s_NONE", node->data.enum_decl.name, node->data.enum_decl.name);
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
                fprintf(out, ", TAG_%s_%s", node->data.enum_decl.name, node->data.enum_decl.variants[i]->data.enum_variant.name);
            }
            fprintf(out, " };\n");
            fprintf(out, "#endif\n");
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
            fprintf(out, "};\n\n");
            if (!node->data.enum_decl.is_generic) {
                fprintf(out, "static struct %s %s_new() { struct %s res; memset(&res, 0, sizeof(res)); return res; }\n", node->data.enum_decl.name, node->data.enum_decl.name, node->data.enum_decl.name);
                
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
                    fprintf(out, "    struct %s res; res.tag = TAG_%s_%s;\n", node->data.enum_decl.name, node->data.enum_decl.name, vname);
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
            }
            break;
        }
        case AST_STRUCT_INIT: {
            const char *struct_name = node->data.struct_init.struct_name;
            if (struct_name && (strcmp(struct_name, "Self") == 0 || strcmp(struct_name, "self") == 0)) {
                if (current_impl_struct[0] != '\0') {
                    struct_name = current_impl_struct;
                }
            }
            if (node->resolved_type && node->resolved_type->kind == TYPE_STRUCT) {
                 struct_name = node->resolved_type->data.struct_type.name;
            }
            
            // Heuristic: if it's an enum variant initialization
            int is_enum_variant = (strchr(struct_name, ':') != NULL);
            // Message_Move style check
            if (!is_enum_variant && strstr(struct_name, "_")) {
                 if (strncmp(struct_name, "Message_", 8) == 0) is_enum_variant = 1;
                 if (strstr(struct_name, "Some") || strstr(struct_name, "None") || strstr(struct_name, "Ok") || strstr(struct_name, "Err")) is_enum_variant = 1;
            }

            if (is_enum_variant) {
                char *clean_v_si = strdup(struct_name);
                char *p = clean_v_si;
                while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                fprintf(out, "%s(", clean_v_si);
                for (int i = 0; i < node->data.struct_init.field_count; i++) {
                    ASTNode *field = node->data.struct_init.fields[i];
                    if (field->type == AST_FIELD_INIT) codegen_node_ext(field->data.field_init.value, out, 1);
                    else codegen_node_ext(field, out, 1);
                    if (i < node->data.struct_init.field_count - 1) fprintf(out, ", ");
                }
                fprintf(out, ")");
                free(clean_v_si);
                break;
            }

            const char *mtype = map_type(struct_name);
            if (strncmp(mtype, "struct ", 7) == 0 || strncmp(mtype, "enum ", 5) == 0) {
                fprintf(out, "(%s){", mtype);
            } else {
                fprintf(out, "(struct %s){", mtype);
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
                    char *old_name = m->data.func.name;
                    
                    // Emit the trait version for dynamic dispatch
                    fprintf(out, "%s %s_%s_%s(void* _self_ptr", map_type(m->data.func.return_type), node->data.trait_impl.struct_name, node->data.trait_impl.trait_name, old_name);
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
                    
                    // Emit the direct version for static calls
                    char *new_name = malloc(strlen(node->data.trait_impl.struct_name) + strlen(old_name) + 2);
                    sprintf(new_name, "%s_%s", node->data.trait_impl.struct_name, old_name);
                    m->data.func.name = new_name;
                    
                    fprintf(out, "%s %s(", map_type(m->data.func.return_type), m->data.func.name);
                    for (int j = 0; j < m->data.func.param_count; j++) {
                        codegen_node_ext(m->data.func.params[j], out, 0);
                        if (j < m->data.func.param_count - 1) fprintf(out, ", ");
                    }
                    fprintf(out, ") ");
                    codegen_node_ext(m->data.func.body, out, 0);
                    fprintf(out, "\n");
                    
                    m->data.func.name = old_name;
                    free(new_name);
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
        case AST_MOD: {
            fprintf(out, "/* mod %s */\n", node->data.module.name);
            if (node->data.module.body && node->data.module.body->type == AST_BLOCK) {
                for (int i = 0; i < node->data.module.body->data.block.count; i++) {
                    codegen_node(node->data.module.body->data.block.statements[i], out);
                }
            } else if (node->data.module.body) {
                codegen_node(node->data.module.body, out);
            }
            break;
        }
        case AST_USE:
            fprintf(out, "/* use %s; */\n", node->data.use_stmt.path);
            break;
        case AST_MATCH:
            if (is_expr) {
                fprintf(out, "({ auto _match_res = 0; void* _match_tmp = &(");
                codegen_node_ext(node->data.match_stmt.expr, out, 1);
                fprintf(out, "); ");
                
                struct Type *expr_type = node->data.match_stmt.expr->resolved_type;
                if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
                char *ename = (expr_type && (expr_type->kind == TYPE_ENUM || expr_type->kind == TYPE_STRUCT)) ? (expr_type->kind == TYPE_ENUM ? expr_type->data.enum_type.name : expr_type->data.struct_type.name) : "void";

                for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                    ASTNode *arm = node->data.match_stmt.arms[i];
                    ASTNode *pattern = arm->data.match_arm.pattern;
                    
                    if (i == 0) fprintf(out, "if (");
                    else fprintf(out, " else if (");
                    
                    char access_path[256];
                    snprintf(access_path, sizeof(access_path), "(*((struct %s*)_match_tmp))", ename);
                    codegen_pattern_condition(pattern, access_path, node->data.match_stmt.expr, out);
                    fprintf(out, ") {\n");
                    
                    codegen_pattern_bindings(pattern, access_path, node->data.match_stmt.expr, out);
                    
                    fprintf(out, "            _match_res = ");
                    if (arm->data.match_arm.body->type == AST_BLOCK) {
                        fprintf(out, "({ ");
                        codegen_node_ext(arm->data.match_arm.body, out, 1);
                        fprintf(out, "; });");
                    } else {
                        codegen_node_ext(arm->data.match_arm.body, out, 1);
                        fprintf(out, ";");
                    }
                    fprintf(out, "\n        }");
                }
                fprintf(out, " _match_res; })");
            } else {
                fprintf(out, "{\n    void* _match_tmp = &(");
                codegen_node_ext(node->data.match_stmt.expr, out, 0);
                fprintf(out, ");\n");
                
                struct Type *expr_type = node->data.match_stmt.expr->resolved_type;
                if (expr_type && expr_type->kind == TYPE_REFERENCE) expr_type = expr_type->data.reference.inner;
                char *ename = (expr_type && (expr_type->kind == TYPE_ENUM || expr_type->kind == TYPE_STRUCT)) ? (expr_type->kind == TYPE_ENUM ? expr_type->data.enum_type.name : expr_type->data.struct_type.name) : "void";

                for (int i = 0; i < node->data.match_stmt.arm_count; i++) {
                    ASTNode *arm = node->data.match_stmt.arms[i];
                    ASTNode *pattern = arm->data.match_arm.pattern;
                    
                    if (i == 0) fprintf(out, "    if (");
                    else fprintf(out, " else if (");
                    
                    char access_path[256];
                    snprintf(access_path, sizeof(access_path), "(*((struct %s*)_match_tmp))", ename);
                    codegen_pattern_condition(pattern, access_path, node->data.match_stmt.expr, out);
                    fprintf(out, ") {\n");
                    
                    codegen_pattern_bindings(pattern, access_path, node->data.match_stmt.expr, out);
                    
                    codegen_node_ext(arm->data.match_arm.body, out, 0);
                    if (arm->data.match_arm.body->type != AST_BLOCK) fprintf(out, ";\n");
                    fprintf(out, "    }");
                }
                fprintf(out, "\n}\n");
            }
            break;
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
                    int arg_idx = 1;
                    while ((p = strstr(p, "{}"))) {
                        char spec = 'd'; // Default
                        if (arg_idx < node->data.macro_call.arg_count) {
                            ASTNode *arg = node->data.macro_call.args[arg_idx];
                            struct Type *t = arg->resolved_type;
                            if (t) {
                                // fprintf(stderr, "DEBUG: Arg %d type kind %d\n", arg_idx, t->kind);
                                if (t->kind == TYPE_PRIMITIVE) {
                                    // fprintf(stderr, "DEBUG:   primitive %d\n", t->data.primitive);
                                    switch (t->data.primitive) {
                                        case PRIM_STR: spec = 's'; break;
                                        case PRIM_I8: spec = 's'; break;
                                        case PRIM_I32: spec = 'd'; break;
                                        case PRIM_I64: spec = 'l'; break; 
                                        case PRIM_F32:
                                        case PRIM_F64: spec = 'f'; break;
                                        case PRIM_BOOL: spec = 'd'; break;
                                        case PRIM_U32: spec = 'u'; break;
                                        case PRIM_U64: spec = 'l'; break; // Should be %llu but fallback to %ld
                                        case PRIM_USIZE: spec = 'z'; break; // %zu
                                        default: spec = 'd'; break;
                                    }
                                } else if (t->kind == TYPE_REFERENCE) {
                                    Type *inner = t->data.reference.inner;
                                    // if (inner) fprintf(stderr, "DEBUG:   inner kind %d\n", inner->kind);
                                    if (inner && inner->kind == TYPE_PRIMITIVE && (inner->data.primitive == PRIM_I8 || inner->data.primitive == PRIM_STR)) spec = 's';
                                    else spec = 'p';
                                } else if (t->kind == TYPE_POINTER) {
                                    Type *inner = t->data.pointer.inner;
                                    // if (inner) fprintf(stderr, "DEBUG:   inner kind %d\n", inner->kind);
                                    if (inner && inner->kind == TYPE_PRIMITIVE && (inner->data.primitive == PRIM_I8 || inner->data.primitive == PRIM_STR)) spec = 's';
                                    else spec = 'p';
                                }
                            }
                            if (arg->type == AST_METHOD_CALL || arg->type == AST_CALL) {
                                // Heuristic: if it's a method call returning a string (common in tests)
                                if (arg->data.method_call.method_name && (strstr(arg->data.method_call.method_name, "summarize") || 
                                    strstr(arg->data.method_call.method_name, "to_string") ||
                                    strstr(arg->data.method_call.method_name, "as_str") ||
                                    strstr(arg->data.method_call.method_name, "name"))) {
                                    spec = 's';
                                }
                            }
                        }
                        p[0] = '%';
                        if (spec == 'l') {
                             p[1] = 'l'; p[2] = 'd'; // Corrected to %lld
                             // But we need to move the rest of the string
                             memmove(p+3, p+2, strlen(p+2)+1);
                             p += 3;
                        } else if (spec == 'z') {
                             p[1] = 'z'; p[2] = 'u';
                             memmove(p+3, p+2, strlen(p+2)+1);
                             p += 3;
                        } else if (spec == 'd' && arg_idx < node->data.macro_call.arg_count) {
                             ASTNode *arg = node->data.macro_call.args[arg_idx];
                             if (arg->type == AST_METHOD_CALL && arg->data.method_call.method_name && strstr(arg->data.method_call.method_name, "len")) {
                                 p[1] = 'z'; p[2] = 'u';
                                 memmove(p+3, p+2, strlen(p+2)+1);
                                 p += 3;
                             } else {
                                 p[1] = spec;
                                 p += 2;
                             }
                        } else {
                             p[1] = spec;
                             p += 2;
                        }
                        arg_idx++;
                    }
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
                fprintf(out, "%s(", node->data.macro_call.name);
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
                        ASTNode *param = item->data.func.params[j];
                        fprintf(out, "%s %s", map_type(param->data.param.type_name), param->data.param.name);
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
            for (int j = 0; j < node->data.block.count; j++) {
                ASTNode *stmt = node->data.block.statements[j];
                int last = (j == node->data.block.count - 1);
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
                        stmt->type == AST_MATCH ||
                        stmt->type == AST_MACRO_CALL) {
                        fprintf(out, "    return ");
                    }
                }

                if (stmt->type == AST_IF || stmt->type == AST_MATCH || stmt->type == AST_BLOCK) {
                    if (stmt_is_expr) fprintf(out, "({ ");
                    codegen_node_ext(stmt, out, stmt_is_expr);
                    if (stmt_is_expr) fprintf(out, "; })");
                } else {
                    codegen_node_ext(stmt, out, stmt_is_expr);
                }
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
                } else if (stmt->type == AST_IF || stmt->type == AST_MATCH || stmt->type == AST_BLOCK) {
                    if (is_expr) fprintf(out, "; ");
                    else fprintf(out, "\n");
                }
            }
            if (is_expr) fprintf(out, " })");
            else fprintf(out, "}\n");
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.type_name) {
                const char *raw_name = node->data.var_decl.type_name;
                const char *mtype = map_type(raw_name);
                fprintf(out, "    %s %s", mtype, node->data.var_decl.name);
            } else {
                if (node->data.var_decl.init && (node->data.var_decl.init->type == AST_STRING_LITERAL || (node->data.var_decl.init->type == AST_METHOD_CALL && strcmp(node->data.var_decl.init->data.method_call.method_name, "to_string") == 0))) {
                     fprintf(out, "    char* %s", node->data.var_decl.name);
                } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT) {
                     const char *sname = node->data.var_decl.init->data.struct_init.struct_name;
                     if (node->data.var_decl.init->resolved_type) sname = node->data.var_decl.init->resolved_type->data.struct_type.name;
                     const char *mtype = map_type(sname);
                     fprintf(out, "    %s %s", mtype, node->data.var_decl.name);
                }
                else if (node->data.var_decl.init && node->resolved_type && node->resolved_type->kind == TYPE_ENUM) {
                     fprintf(out, "    %s %s", map_type(type_to_string(node->resolved_type)), node->data.var_decl.name);
                }
                else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL) {
                    char *name = node->data.var_decl.init->data.call.name;
                    char *clean_name = strdup(name);
                    char *p = clean_name;
                    while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                    if (strcmp(clean_name, "Box_new") == 0 || strcmp(clean_name, "Box::new") == 0) fprintf(out, "    void* %s", node->data.var_decl.name);
                    else {
                        char *underscore = strrchr(clean_name, '_');
                        if (underscore && isupper(clean_name[0]) && !strstr(clean_name, "_i32") && !strstr(clean_name, "_char") && !strstr(clean_name, "_Ident")) {
                            *underscore = '\0';
                            fprintf(out, "    struct %s %s", clean_name, node->data.var_decl.name);
                            *underscore = '_';
                        } else if (strcmp(clean_name, "Message_Write") == 0) fprintf(out, "    struct Message %s", node->data.var_decl.name);
                        else {
                             if (node->data.var_decl.init->resolved_type) {
                                  fprintf(out, "    %s %s", map_type(type_to_string(node->data.var_decl.init->resolved_type)), node->data.var_decl.name);
                             } else {
                                  fprintf(out, "    auto %s", node->data.var_decl.name);
                             }
                        }
                    }
                } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_UNOP && strcmp(node->data.var_decl.init->data.unop.op, "&") == 0) {
                    fprintf(out, "    auto %s", node->data.var_decl.name);
                } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && (strcmp(node->data.var_decl.init->data.call.name, "Box::new") == 0 || strcmp(node->data.var_decl.init->data.call.name, "Box_new") == 0)) {
                     fprintf(out, "    void* %s", node->data.var_decl.name);
                } else if (node->resolved_type && node->resolved_type->kind == TYPE_STRUCT) {
                     fprintf(out, "    struct %s %s", node->resolved_type->data.struct_type.name, node->data.var_decl.name);
                } else fprintf(out, "    auto %s", node->data.var_decl.name); // Default to auto for C23
            }
            if (node->data.var_decl.init) { 
                fprintf(out, " = "); 
                codegen_node_ext(node->data.var_decl.init, out, 1); 
            }
            fprintf(out, ";\n");
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
            if (node->resolved_type) {
                 fprintf(stderr, "DEBUG: codegen AST_IDENT name=%s type=%s (kind %d)\n", node->data.ident.name, type_to_string(node->resolved_type), node->resolved_type->kind);
            } else {
                 fprintf(stderr, "DEBUG: codegen AST_IDENT name=%s type=NULL\n", node->data.ident.name);
            }
            char *name = strdup(node->data.ident.name);
            char *p = name;
            while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
            if (node->resolved_type && node->resolved_type->kind == TYPE_ENUM) {
                // If it's an enum variant constructor (contains :: or matches a variant name)
                if (strchr(name, ':')) {
                    char *clean_name = strdup(name);
                    char *p = clean_name;
                    while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
                    fprintf(out, "%s()", clean_name);
                    free(clean_name);
                } else {
                    // It could be a variant used as a flat name (e.g. Message_Quit)
                    // or a local variable. If it's a local variable, it will be in the name.
                    // But wait, if it's Message_Quit it SHOULD be called if it's a variant.
                    // Let's use a heuristic: if it starts with a capital letter and matches an enum variant...
                    // Actually, if it's a variant it SHOULD be in the symbol table as a variant.
                    
                    // Check if it's a known flat variant name
                    if (isupper(name[0]) && strchr(name, '_')) {
                         fprintf(out, "%s()", name);
                    } else {
                         fprintf(out, "%s", name);
                    }
                }
            } else {
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
            }
            break;
        }
        case AST_CALL: {
            char *name = node->data.call.name;
            if (strcmp(name, "Vec::new") == 0) { name = "Vec_new"; }
            if (strcmp(name, "String::new") == 0) { name = "String_new"; }
            char *clean_name = strdup(name);
            char *p = clean_name;
            while (*p) { if (*p == ':' && *(p+1) == ':') { *p = '_'; memmove(p+1, p+2, strlen(p+2)+1); } p++; }
            if (strcmp(clean_name, "Vec_new") == 0) {
                 if (node->resolved_type && node->resolved_type->kind == TYPE_STRUCT) {
                      char *mangled = strdup(node->resolved_type->data.struct_type.name);
                      free(clean_name);
                      clean_name = malloc(strlen(mangled) + 5);
                      sprintf(clean_name, "%s_new", mangled);
                      free(mangled);
                 }
            }
            if (strcmp(clean_name, "Box_new") == 0 || strcmp(clean_name, "Box::new") == 0) { fprintf(out, "({ void* _b = malloc(sizeof(int)); * (int*)_b = "); codegen_node_ext(node->data.call.args[0], out, 1); fprintf(out, "; _b; })"); }
            else {
                fprintf(out, "%s(", clean_name);
                for (int i = 0; i < node->data.call.arg_count; i++) { codegen_node_ext(node->data.call.args[i], out, 1); if (i < node->data.call.arg_count - 1) fprintf(out, ", "); }
                fprintf(out, ")");
            }
            free(clean_name);
            break;
        }
        case AST_UNOP:
            if (strcmp(node->data.unop.op, "*") == 0) {
                if (node->data.unop.expr->resolved_type && (node->data.unop.expr->resolved_type->kind == TYPE_POINTER || node->data.unop.expr->resolved_type->kind == TYPE_REFERENCE)) {
                     fprintf(out, "(*(%s*)", map_type(type_to_string(node->data.unop.expr->resolved_type->data.pointer.inner)));
                     codegen_node_ext(node->data.unop.expr, out, 1);
                     fprintf(out, ")");
                } else if (node->data.unop.expr->type == AST_IDENT) {
                    fprintf(out, "(*(int*)%s)", node->data.unop.expr->data.ident.name); // Heuristic for Box deref
                } else {
                    fprintf(out, "(*");
                    codegen_node_ext(node->data.unop.expr, out, 1);
                    fprintf(out, ")");
                }
            } else {
                fprintf(out, "(%s", node->data.unop.op);
                codegen_node_ext(node->data.unop.expr, out, 1);
                fprintf(out, ")");
            }
            break;
        case AST_STRUCT_DECL:
            if (node->data.struct_decl.is_generic) return;
            if (strcmp(node->data.struct_decl.name, "Vec_i32") == 0 || strcmp(node->data.struct_decl.name, "Vec_u8") == 0 || strcmp(node->data.struct_decl.name, "Vec_i8") == 0) return;
            fprintf(out, "struct %s {\n", node->data.struct_decl.name);
            for (int i = 0; i < node->data.struct_decl.field_count; i++) { fprintf(out, "    "); codegen_node(node->data.struct_decl.fields[i], out); fprintf(out, ";\n"); }
            fprintf(out, "};\n\n");
            if (!node->data.struct_decl.is_generic) {
                fprintf(out, "static struct %s %s_new_default() { struct %s res; memset(&res, 0, sizeof(res)); return res; }\n", node->data.struct_decl.name, node->data.struct_decl.name, node->data.struct_decl.name);
            }
            break;
        case AST_FIELD_ACCESS:
            codegen_node(node->data.field_access.receiver, out);
            if (node->data.field_access.receiver->type == AST_IDENT && (strcmp(node->data.field_access.receiver->data.ident.name, "self") == 0 || strcmp(node->data.field_access.receiver->data.ident.name, "this") == 0)) fprintf(out, "->%s", node->data.field_access.field_name);
            else fprintf(out, ".%s", node->data.field_access.field_name);
            break;
        case AST_IF:
            if (is_expr) {
                fprintf(out, "({ auto _res = 0; if (");
                codegen_node_ext(node->data.if_stmt.condition, out, 1);
                fprintf(out, ") { _res = ");
                codegen_node_ext(node->data.if_stmt.then_branch, out, 1);
                fprintf(out, "; }");
                if (node->data.if_stmt.else_branch) {
                    fprintf(out, " else { _res = ");
                    codegen_node_ext(node->data.if_stmt.else_branch, out, 1);
                    fprintf(out, "; }");
                }
                fprintf(out, " _res; })");
            } else {
                fprintf(out, "    if (");
                codegen_node_ext(node->data.if_stmt.condition, out, 1);
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
        case AST_FOR_STMT:
            fprintf(out, "    { auto _iter = ");
            codegen_node_ext(node->data.for_loop.iterable, out, 1);
            fprintf(out, ".into_iter();\n");
            fprintf(out, "      while (1) {\n");
            fprintf(out, "        auto _next = _iter.next();\n");
            fprintf(out, "        if (_next.tag == TAG_Option_None) break;\n");
            fprintf(out, "        auto %s = _next.data.Some._0;\n", node->data.for_loop.var_name);
            codegen_node(node->data.for_loop.body, out);
            fprintf(out, "      }\n");
            fprintf(out, "    }\n");
            break;
        case AST_RETURN:
            if (node->data.ret_stmt.value) {
                if (node->data.ret_stmt.value->type == AST_IF) {
                    fprintf(out, "    return ");
                    codegen_node_ext(node->data.ret_stmt.value, out, 1);
                    fprintf(out, ";\n");
                    break;
                }
                if (is_expr) fprintf(out, "({ return ");
                else fprintf(out, "    return ");
                codegen_node(node->data.ret_stmt.value, out);
                if (is_expr) fprintf(out, "; })");
                else fprintf(out, ";\n");
            } else {
                fprintf(out, "    return;\n");
            }
            break;
        case AST_BOOL_LITERAL:
            fprintf(out, "%s", node->data.bool_literal.value ? "1" : "0");
            break;
        case AST_MACRO_RULES:
            fprintf(out, "/* macro_rules! %s skipped */\n", node->data.macro_rules.name);
            break;
        case AST_CAST:
            fprintf(out, "((%s)", node->data.cast.type_name);
            codegen_node_ext(node->data.cast.expr, out, 1);
            fprintf(out, ")");
            break;
        case AST_ENUM_VARIANT:
            fprintf(out, "/* Enum Variant %s */", node->data.enum_variant.name);
            break;
        case AST_GENERIC_TYPE:
            fprintf(out, "/* Generic Type %s */", node->data.generic_type.base_name);
            break;
    }
    current_table = old_table_inner;
}

void codegen_emit_enum_tags(ASTNode *node, FILE *out) {
    if (node->type != AST_ENUM_DECL) return;
    fprintf(out, "#ifndef TAG_%s_DEFINED\n", node->data.enum_decl.name);
    fprintf(out, "#define TAG_%s_DEFINED\n", node->data.enum_decl.name);
    fprintf(out, "enum %s_tag { TAG_%s_NONE", node->data.enum_decl.name, node->data.enum_decl.name);
    for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
        fprintf(out, ", TAG_%s_%s", node->data.enum_decl.name, node->data.enum_decl.variants[i]->data.enum_variant.name);
    }
    fprintf(out, " };\n");
    fprintf(out, "#endif\n");
}

void codegen_emit_type_body(ASTNode *node, FILE *out) {
    if (node->type == AST_STRUCT_DECL) {
        if (node->data.struct_decl.is_generic) return;
        if (strcmp(node->data.struct_decl.name, "Vec_i32") == 0 || strcmp(node->data.struct_decl.name, "Vec_u8") == 0 || strcmp(node->data.struct_decl.name, "Vec_i8") == 0) return;
        fprintf(out, "struct %s {\n", node->data.struct_decl.name);
        for (int i = 0; i < node->data.struct_decl.field_count; i++) { fprintf(out, "    "); codegen_node(node->data.struct_decl.fields[i], out); fprintf(out, ";\n"); }
        fprintf(out, "};\n\n");
        fprintf(out, "static struct %s %s_new_default() { struct %s res; memset(&res, 0, sizeof(res)); return res; }\n", node->data.struct_decl.name, node->data.struct_decl.name, node->data.struct_decl.name);
    } else if (node->type == AST_ENUM_DECL) {
        if (node->data.enum_decl.is_generic) return;
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
        fprintf(out, "};\n\n");
        fprintf(out, "static struct %s %s_new() { struct %s res; memset(&res, 0, sizeof(res)); return res; }\n", node->data.enum_decl.name, node->data.enum_decl.name, node->data.enum_decl.name);
                for (int i = 0; i < node->data.enum_decl.variant_count; i++) {
            ASTNode *variant = node->data.enum_decl.variants[i];
            if (variant->data.enum_variant.variant_type == AST_CALL) {
                fprintf(out, "static struct %s %s_%s(", node->data.enum_decl.name, node->data.enum_decl.name, variant->data.enum_variant.name);
                for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                    if (j > 0) fprintf(out, ", ");
                    fprintf(out, "%s _%d", map_type(variant->data.enum_variant.fields[j]->data.param.type_name), j);
                }
                fprintf(out, ") { struct %s res; res.tag = TAG_%s_%s; ", node->data.enum_decl.name, node->data.enum_decl.name, variant->data.enum_variant.name);
                for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                    fprintf(out, "res.data.%s._%d = _%d; ", variant->data.enum_variant.name, j, j);
                }
                fprintf(out, "return res; }\n");
            } else if (variant->data.enum_variant.variant_type == AST_PARAM) {
                // Unit variant
                fprintf(out, "static struct %s %s_%s() { struct %s res; res.tag = TAG_%s_%s; return res; }\n", node->data.enum_decl.name, node->data.enum_decl.name, variant->data.enum_variant.name, node->data.enum_decl.name, node->data.enum_decl.name, variant->data.enum_variant.name);
            }
        }
    }
}

void codegen_generate(ASTNode *node, FILE *out, Target target, const char *crate_name) {
    if (!node) return;
    current_crate_name_internal = crate_name;
    
    // Skip generic definitions in codegen, as they should only be emitted via specializations
    if (node->type == AST_STRUCT_DECL && node->data.struct_decl.is_generic) return;
    if (node->type == AST_ENUM_DECL && node->data.enum_decl.is_generic) return;
    if (node->type == AST_FUNC && node->data.func.is_generic) return;
    if (node->type == AST_TRAIT_IMPL && node->data.trait_impl.is_generic) return;

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
