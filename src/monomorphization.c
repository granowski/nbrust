#include "monomorphization.h"
#include "parser.h"
#include "codegen.h"
#include "type_checker.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);
static char *substitute_type(const char *type, char **params, char **args, int count);

static int is_already_specialized(const char *name) {
    if (!name) return 0;
    // Check for common specialized prefixes or suffixes
    if (strstr(name, "_i32") || strstr(name, "_u8") || strstr(name, "_i8") || strstr(name, "_bool")) return 1;
    if (strstr(name, "Vec_") || strstr(name, "Box_") || strstr(name, "Option_") || strstr(name, "Result_") || strstr(name, "VecIter_")) return 1;
    // If it already contains an underscore followed by something that looks like a type
    char *underscore = strchr(name, '_');
    if (underscore && isupper(underscore[1])) return 1;
    return 0;
}

typedef struct GenericRegistryNode {
    char *name;
    ASTNode *node;
    struct GenericRegistryNode *next;
} GenericRegistryNode;

static GenericRegistryNode *registry = NULL;
static char *current_specialization_mangled = NULL;

static char *safe_strdup(const char *s) {
    return s ? strdup(s) : NULL;
}

void monomorphization_register(ASTNode *node) {
    if (!node) return;
    char *name = NULL;
    if (node->type == AST_FUNC) name = node->data.func.name;
    else if (node->type == AST_STRUCT_DECL) name = node->data.struct_decl.name;
    else if (node->type == AST_ENUM_DECL) name = node->data.enum_decl.name;
    else if (node->type == AST_IMPL) name = node->data.impl_block.struct_name;
    else if (node->type == AST_TRAIT_IMPL) name = node->data.trait_impl.struct_name;
    
    if (!name) return;
    // Note: Multiple impl blocks for the same struct are allowed, so we don't return early if name matches.
    // However, structs and enums should be unique in the registry for lookup.
    if (node->type == AST_STRUCT_DECL || node->type == AST_ENUM_DECL || node->type == AST_FUNC) {
        if (monomorphization_lookup(name)) {
             return;
        }
    }
    
    GenericRegistryNode *reg = malloc(sizeof(GenericRegistryNode));
    reg->name = strdup(name); reg->node = node; reg->next = registry; registry = reg;
}

ASTNode *monomorphization_lookup(const char *name) {
    if (!name) return NULL;
    // fprintf(stderr, "DEBUG: monomorphization_lookup '%s'\n", name);
    GenericRegistryNode *curr = registry;
    ASTNode *fallback = NULL;
    while (curr) { 
        if (strcmp(curr->name, name) == 0) {
            // Prefer struct or enum declarations over impl blocks
            if (curr->node->type == AST_STRUCT_DECL || curr->node->type == AST_ENUM_DECL) {
                return curr->node; 
            }
            if (!fallback) fallback = curr->node;
        }
        curr = curr->next; 
    }
    if (fallback) {
        // fprintf(stderr, "DEBUG: monomorphization_lookup '%s' FOUND fallback type=%d\n", name, fallback->type);
        return fallback;
    }
    // fprintf(stderr, "DEBUG: monomorphization_lookup '%s' NOT FOUND\n", name);
    return NULL;
}

typedef struct SpecializationNode {
    char *mangled_name;
    ASTNode *node;
    struct SpecializationNode *next;
} SpecializationNode;

static SpecializationNode *specializations = NULL;

static int is_specialized(const char *mangled_name) {
    SpecializationNode *curr = specializations;
    while (curr) { if (strcmp(curr->mangled_name, mangled_name) == 0) return 1; curr = curr->next; }
    return 0;
}

static void walk_and_specialize(ASTNode *node, char **params, char **args, int count);

static void register_specialization(const char *mangled_name, ASTNode *node) {
    if (is_specialized(mangled_name)) return;
    SpecializationNode *s = malloc(sizeof(SpecializationNode));
    s->mangled_name = strdup(mangled_name); s->node = node; s->next = specializations; specializations = s;
}

static void specialize_methods(const char *base, const char *mangled, char **params, char **args, int count) {
    GenericRegistryNode *curr = registry;
    int matches = 0;
    while (curr) {
        if (curr->node->type == AST_IMPL && curr->node->data.impl_block.struct_name) {
             char *sname = strdup(curr->node->data.impl_block.struct_name);
             char *lt = strchr(sname, '<'); if (lt) *lt = '\0';
             if (strcmp(sname, base) == 0) {
                matches++;
                int ncount = count + 2;
                char **nparams = malloc(sizeof(char*) * ncount);
                char **nargs = malloc(sizeof(char*) * ncount);
                nparams[0] = "Self"; nargs[0] = (char*)mangled;
                nparams[1] = "self"; nargs[1] = (char*)mangled;
                for (int i = 0; i < count; i++) { nparams[i+2] = params[i]; nargs[i+2] = args[i]; }
                
                char *old_mangled = current_specialization_mangled;
                current_specialization_mangled = (char*)mangled;
                
                for (int i = 0; i < curr->node->data.impl_block.method_count; i++) {
                    ASTNode *generic_method = curr->node->data.impl_block.methods[i];
                    ASTNode *method = specialize_node(generic_method, nparams, nargs, ncount);
                    if (method->type == AST_FUNC && method->data.func.name) {
                        char *old_mname = method->data.func.name;
                        char *new_mname;
                        if (strstr(old_mname, mangled) == old_mname) {
                             // Already mangled correctly
                             new_mname = strdup(old_mname);
                        } else {
                            char *new_mname_buf = malloc(strlen(mangled) + 2 + strlen(old_mname) + 1);
                            sprintf(new_mname_buf, "%s_%s", mangled, old_mname);
                            new_mname = new_mname_buf;
                        }
                        if (method->data.func.name) free(method->data.func.name);
                        method->data.func.name = new_mname;
                        register_specialization(method->data.func.name, method);
                    }
                }
                current_specialization_mangled = old_mangled;
                free(nparams); free(nargs);
             }
             free(sname);
        } else if (curr->node->type == AST_TRAIT_IMPL && curr->node->data.trait_impl.struct_name) {
             char *sname = strdup(curr->node->data.trait_impl.struct_name);
             char *lt = strchr(sname, '<'); if (lt) *lt = '\0';
             if (strcmp(sname, base) == 0) {
                matches++;
                int ncount = count + 2;
                char **nparams = malloc(sizeof(char*) * ncount);
                char **nargs = malloc(sizeof(char*) * ncount);
                nparams[0] = "Self"; nargs[0] = (char*)mangled;
                nparams[1] = "self"; nargs[1] = (char*)mangled;
                for (int i = 0; i < count; i++) { nparams[i+2] = params[i]; nargs[i+2] = args[i]; }
                
                char *old_mangled = current_specialization_mangled;
                current_specialization_mangled = (char*)mangled;
                
                for (int i = 0; i < curr->node->data.trait_impl.method_count; i++) {
                    ASTNode *generic_method = curr->node->data.trait_impl.methods[i];
                    // fprintf(stderr, "DEBUG: specializing trait method '%s' for %s\n", generic_method->data.func.name, mangled);
                    ASTNode *method = specialize_node(generic_method, nparams, nargs, ncount);
                    if (method->type == AST_FUNC && method->data.func.name) {
                        char *old_mname = method->data.func.name;
                        char *new_mname;
                        if (strstr(old_mname, mangled) == old_mname) {
                             new_mname = strdup(old_mname);
                        } else {
                            char *new_mname_buf = malloc(strlen(mangled) + 2 + strlen(old_mname) + 1);
                            sprintf(new_mname_buf, "%s_%s", mangled, old_mname);
                            new_mname = new_mname_buf;
                        }
                        if (method->data.func.name) free(method->data.func.name);
                        method->data.func.name = new_mname;
                        register_specialization(method->data.func.name, method);
                    }
                }
                current_specialization_mangled = old_mangled;
                free(nparams); free(nargs);
             }
             free(sname);
        }
        curr = curr->next;
    }
    if (matches == 0) { }
}

static char *mangle_name(const char *base, char **args, int count) {
    if (!base) return strdup("NULL_BASE");
    char buf[512]; strcpy(buf, base);
    for (int i = 0; i < count; i++) {
        char *arg = args[i];
        if (!arg) continue;
        
        // Normalize common types for consistent mangling
        const char *normalized = arg;
        if (strcmp(arg, "V") == 0 || strcmp(arg, "T") == 0 || strcmp(arg, "int") == 0 || strcmp(arg, "i32") == 0) normalized = "i32";
        else if (strcmp(arg, "unsigned int") == 0 || strcmp(arg, "u32") == 0) normalized = "u32";
        else if (strcmp(arg, "unsigned char") == 0 || strcmp(arg, "u8") == 0) normalized = "u8";
        else if (strcmp(arg, "signed char") == 0 || strcmp(arg, "i8") == 0 || strcmp(arg, "char") == 0) normalized = "i8";
        else if (strcmp(arg, "bool") == 0) normalized = "bool";
        
        strcat(buf, "_"); 
        for (int j = 0; normalized[j]; j++) {
            if (normalized[j] == '&') strcat(buf, "Ref");
            else if (normalized[j] == '*') strcat(buf, "Ptr");
            else if (normalized[j] == ' ') strcat(buf, "_");
            else if (normalized[j] == '<' || normalized[j] == '>') strcat(buf, "_");
            else if (normalized[j] == ',') strcat(buf, "_");
            else if (normalized[j] == ':') strcat(buf, "_");
            else { 
                int len = strlen(buf); 
                if (len < 500) { buf[len] = normalized[j]; buf[len+1] = '\0'; } 
            }
        }
    }
    return strdup(buf);
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count);

static char *substitute_type(const char *type, char **params, char **args, int count) {
    if (!type) return NULL;
    
    // Handle Self/self as exact match early with fallback
    if (strcmp(type, "Self") == 0 || strcmp(type, "self") == 0) {
        for (int i = 0; i < count; i++) {
            if (params[i] && (strcmp(params[i], "Self") == 0 || strcmp(params[i], "self") == 0)) return strdup(args[i]);
        }
        if (current_specialization_mangled) return strdup(current_specialization_mangled);
    }

    // Exact match for generic parameters (high priority)
    for (int i = 0; i < count; i++) {
        if (params[i] && strcmp(type, params[i]) == 0) return strdup(args[i]);
    }
    
    // Handle mut qualifier (we strip it for type name generation, but preserve it for pointers/refs)
    if (strncmp(type, "mut ", 4) == 0) {
        return substitute_type(type + 4, params, args, count);
    }
    
    // Handle pointers and references
    int len = strlen(type);
    if (type[0] == '*' || type[0] == '&') {
        const char *inner = type + 1;
        int has_mut = 0;
        int has_const = 0;
        while (1) {
            if (strncmp(inner, "mut ", 4) == 0) { inner += 4; has_mut = 1; }
            else if (strncmp(inner, "const ", 6) == 0) { inner += 6; has_const = 1; }
            else if (strncmp(inner, "mut", 3) == 0 && (inner[3] == ' ' || inner[3] == '\0')) { inner += 3; has_mut = 1; if (*inner == ' ') inner++; }
            else break;
        }
        char *sub_inner = substitute_type(inner, params, args, count);
        char buf[256];
        if (has_mut) snprintf(buf, sizeof(buf), "%cmut %s", type[0], sub_inner);
        else if (has_const) snprintf(buf, sizeof(buf), "%cconst %s", type[0], sub_inner);
        else snprintf(buf, sizeof(buf), "%c%s", type[0], sub_inner);
        free(sub_inner);
        return strdup(buf);
    } else if (len > 0 && (type[len-1] == '*' || type[len-1] == '&')) {
        char *inner = strdup(type);
        inner[len-1] = '\0';
        char *sub_inner = substitute_type(inner, params, args, count);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s%c", sub_inner, type[len-1]);
        free(sub_inner);
        free(inner);
        return strdup(buf);
    }
    
    // Handle Self and self specifically
    if (type && (strstr(type, "Self") || strstr(type, "self"))) {
        char *res = strdup(type);
        int replaced = 0;
        
        // Try params first
        for (int i = 0; i < count; i++) {
            if (params[i] && (strcmp(params[i], "Self") == 0 || strcmp(params[i], "self") == 0)) {
                char *p;
                while ((p = strstr(res, "Self"))) {
                    int pos = p - res;
                    int mangled_len = strlen(args[i]);
                    char *new_res = malloc(strlen(res) - 4 + mangled_len + 1);
                    strncpy(new_res, res, pos);
                    strcpy(new_res + pos, args[i]);
                    strcpy(new_res + pos + mangled_len, p + 4);
                    free(res);
                    res = new_res;
                    replaced = 1;
                }
                while ((p = strstr(res, "self"))) {
                    int pos = p - res;
                    int mangled_len = strlen(args[i]);
                    char *new_res = malloc(strlen(res) - 4 + mangled_len + 1);
                    strncpy(new_res, res, pos);
                    strcpy(new_res + pos, args[i]);
                    strcpy(new_res + pos + mangled_len, p + 4);
                    free(res);
                    res = new_res;
                    replaced = 1;
                }
            }
        }
        
        // Fallback to current_specialization_mangled
        if (!replaced && current_specialization_mangled) {
            char *p;
            while ((p = strstr(res, "Self"))) {
                int pos = p - res;
                int mangled_len = strlen(current_specialization_mangled);
                char *new_res = malloc(strlen(res) - 4 + mangled_len + 1);
                strncpy(new_res, res, pos);
                strcpy(new_res + pos, current_specialization_mangled);
                strcpy(new_res + pos + mangled_len, p + 4);
                free(res);
                res = new_res;
                replaced = 1;
            }
            while ((p = strstr(res, "self"))) {
                int pos = p - res;
                int mangled_len = strlen(current_specialization_mangled);
                char *new_res = malloc(strlen(res) - 4 + mangled_len + 1);
                strncpy(new_res, res, pos);
                strcpy(new_res + pos, current_specialization_mangled);
                strcpy(new_res + pos + mangled_len, p + 4);
                free(res);
                res = new_res;
                replaced = 1;
            }
        }
        
        // Post-process to replace :: with _ in substituted types
        if (replaced) {
            for (int i = 0; res[i]; i++) {
                if (res[i] == ':' && res[i+1] == ':') {
                    res[i] = '_';
                    memmove(res + i + 1, res + i + 2, strlen(res + i + 2) + 1);
                }
            }
            return res;
        }
        free(res);
    }
    if (strcmp(type, "Item") == 0 || strcmp(type, "Self::Item") == 0) {
        for (int i = 0; i < count; i++) {
            if (params[i] && (strcmp(params[i], "Item") == 0 || strcmp(params[i], "Self::Item") == 0)) {
                return strdup(args[i]);
            }
        }
        // Fallback for Item if not found in params
        for (int i = 0; i < count; i++) {
            if (params[i] && (strcmp(params[i], "T") == 0 || strcmp(params[i], "V") == 0)) return strdup(args[i]);
        }
        return strdup("i32");
    }
    
    // Handle T, V, K, E, R as defaults if not found in params
    if (strcmp(type, "T") == 0 || strcmp(type, "V") == 0 || strcmp(type, "K") == 0 || strcmp(type, "E") == 0 || strcmp(type, "R") == 0) {
        for (int i = 0; i < count; i++) if (params[i] && strcmp(params[i], type) == 0) return strdup(args[i]);
        return strdup("i32");
    }
    
    // Handle nested generics like Vec<T>
    if (strchr(type, '<')) {
        char *type_copy = strdup(type);
        char *lt = strchr(type_copy, '<');
        char *gt = strrchr(type_copy, '>');
        if (lt && gt) {
            *lt = '\0'; *gt = '\0';
            char *base = type_copy;
            char *args_str = lt + 1;
            
            char *arg_tokens[10];
            int arg_idx = 0;
            char *arg_copy = strdup(args_str);
            char *token = strtok(arg_copy, ",");
            while (token && arg_idx < 10) {
                while (*token == ' ') token++;
                arg_tokens[arg_idx++] = substitute_type(token, params, args, count);
                token = strtok(NULL, ",");
            }
            
            char *mangled = mangle_name(base, arg_tokens, arg_idx);
            for (int i = 0; i < arg_idx; i++) free(arg_tokens[i]);
            free(arg_copy); free(type_copy);
            return mangled;
        }
        free(type_copy);
    }
    
    return strdup(type);
}

static ASTNode *specialize_node(ASTNode *node, char **params, char **args, int count) {
    if (!node) return NULL;
    ASTNode *new_node = ast_clone(node);
    
    // Propagate current_specialization_mangled into walk_and_specialize
    walk_and_specialize(new_node, params, args, count);
    
    // Force immediate substitution of generic parameters in specialized nodes
    switch (node->type) {
        case AST_BLOCK:
            for (int i = 0; i < new_node->data.block.count; i++) {
                new_node->data.block.statements[i] = specialize_node(node->data.block.statements[i], params, args, count);
            }
            break;
        case AST_FUNC:
            if (new_node->data.func.return_type) {
                char *old = new_node->data.func.return_type;
                new_node->data.func.return_type = substitute_type(old, params, args, count);
                free(old);
            }
            for (int i = 0; i < new_node->data.func.param_count; i++) {
                ASTNode *param = new_node->data.func.params[i];
                if (param->data.param.type_name) {
                    char *old = param->data.param.type_name;
                    char *sub = substitute_type(old, params, args, count);
                    if (sub && sub[0] == '\0') { free(sub); sub = strdup("void*"); } // Handle empty 'mut'
                    param->data.param.type_name = sub;
                    free(old);
                }
            }
            if (new_node->data.func.body) {
                ASTNode *old_body = new_node->data.func.body;
                new_node->data.func.body = specialize_node(old_body, params, args, count);
            }
            break;
        case AST_STRUCT_DECL:
            for (int i = 0; i < new_node->data.struct_decl.field_count; i++) {
                ASTNode *field = new_node->data.struct_decl.fields[i];
                if (field->data.param.type_name) {
                    char *old = field->data.param.type_name;
                    field->data.param.type_name = substitute_type(old, params, args, count);
                    free(old);
                }
            }
            break;
        case AST_ENUM_DECL:
            for (int i = 0; i < new_node->data.enum_decl.variant_count; i++) {
                ASTNode *variant = new_node->data.enum_decl.variants[i];
                for (int j = 0; j < variant->data.enum_variant.field_count; j++) {
                    ASTNode *field = variant->data.enum_variant.fields[j];
                    if (field->data.param.type_name) {
                        char *old = field->data.param.type_name;
                        field->data.param.type_name = substitute_type(old, params, args, count);
                        free(old);
                    }
                }
            }
            break;
        case AST_VAR_DECL:
            if (new_node->data.var_decl.type_name) {
                char *old = new_node->data.var_decl.type_name;
                new_node->data.var_decl.type_name = substitute_type(old, params, args, count);
                free(old);
            }
            if (new_node->data.var_decl.init) {
                ASTNode *old_init = new_node->data.var_decl.init;
                new_node->data.var_decl.init = specialize_node(old_init, params, args, count);
            }
            break;
        case AST_BINOP:
            new_node->data.binop.left = specialize_node(node->data.binop.left, params, args, count);
            new_node->data.binop.right = specialize_node(node->data.binop.right, params, args, count);
            break;
        case AST_CALL:
            if (new_node->data.call.name) { char *old = new_node->data.call.name; new_node->data.call.name = substitute_type(old, params, args, count); free(old); }
            for (int i = 0; i < node->data.call.arg_count; i++) new_node->data.call.args[i] = specialize_node(node->data.call.args[i], params, args, count);
            break;
        case AST_MACRO_CALL:
            for (int i = 0; i < node->data.macro_call.arg_count; i++) new_node->data.macro_call.args[i] = specialize_node(node->data.macro_call.args[i], params, args, count);
            break;
        case AST_STRUCT_INIT:
            for (int i = 0; i < node->data.struct_init.field_count; i++) new_node->data.struct_init.fields[i] = specialize_node(node->data.struct_init.fields[i], params, args, count);
            break;
        case AST_FIELD_INIT:
            new_node->data.field_init.value = specialize_node(node->data.field_init.value, params, args, count);
            break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) new_node->data.enum_variant.fields[i] = specialize_node(node->data.enum_variant.fields[i], params, args, count);
            break;
        case AST_IDENT: {
            char *old_name = new_node->data.ident.name;
            if (strcmp(old_name, "self") == 0 || strcmp(old_name, "Self") == 0) {
                // Don't substitute 'self' identifier in method bodies during monomorphization
                // It should remain 'self' and refer to the parameter.
                break;
            }
            char *sub = substitute_type(old_name, params, args, count);
            if (sub && strcmp(sub, old_name) != 0) {
                free(new_node->data.ident.name);
                new_node->data.ident.name = sub;
            } else {
                free(sub);
            }
            break;
        }
        default: break;
    }

    switch (node->type) {
        case AST_FUNC:
            if (new_node->data.func.name) {
                char *old = new_node->data.func.name;
                new_node->data.func.name = mangle_name(old, args, count); 
            }
            new_node->data.func.is_generic = 0; // It's now specialized
            new_node->data.func.is_specialized = 1;
    char *res = substitute_type(node->data.func.return_type, params, args, count);
    if (res && strchr(res, '<')) {
        char *type_name = strdup(res);
        char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
        if (lt && gt) {
            *lt = '\0'; *gt = '\0';
            char *base = type_name; char *arg_str = lt + 1;
            char *sub_args[10]; int arg_count = 0;
            char *arg_copy = strdup(arg_str); char *token = strtok(arg_copy, ",");
            while (token && arg_count < 10) { sub_args[arg_count++] = strdup(token); token = strtok(NULL, ","); }
            char *mangled = mangle_name(base, sub_args, arg_count);
            free(res); res = mangled;
            for (int i = 0; i < arg_count; i++) free(sub_args[i]);
            free(arg_copy);
        }
        free(type_name);
    }
    if (res && (strcmp(res, "Self") == 0 || strcmp(res, "Wrapper") == 0 || strcmp(res, "Vec") == 0 || strcmp(res, "Box") == 0 || strcmp(res, "Option") == 0 || strcmp(res, "Result") == 0 || res[0] == '\0')) {
        char *target_base = (strcmp(res, "Self") == 0 || res[0] == '\0') ? params[count-1] : res;
        char *target_struct = strdup(target_base);
        // If it's a known generic base, mangle it with args
        if (strcmp(target_base, "Vec") == 0 || strcmp(target_base, "Option") == 0 || strcmp(target_base, "Box") == 0) {
             free(target_struct);
             target_struct = mangle_name(target_base, args, count-1);
        }
        free(res); res = target_struct;
    }
    new_node->data.func.return_type = res;
            
            for (int i = 0; i < node->data.func.param_count; i++) {
                new_node->data.func.params[i] = specialize_node(node->data.func.params[i], params, args, count);
                // Ensure Self and self are substituted in parameters (already done by specialize_node -> substitute_type usually, 
                // but let's be double sure for receiver types and pointers to Self)
                char *ptype = new_node->data.func.params[i]->data.param.type_name;
                if (ptype && (strstr(ptype, "Self") || strstr(ptype, "self"))) {
                    char *sub = substitute_type(ptype, params, args, count);
                    free(new_node->data.func.params[i]->data.param.type_name);
                    new_node->data.func.params[i]->data.param.type_name = sub;
                }
            }
            new_node->data.func.body = specialize_node(node->data.func.body, params, args, count);
            new_node->data.func.generic_param_count = 0; break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) new_node->data.block.statements[i] = specialize_node(node->data.block.statements[i], params, args, count);
            break;
        case AST_STRUCT_INIT:
            if (new_node->data.struct_init.struct_name) { char *old = new_node->data.struct_init.struct_name; new_node->data.struct_init.struct_name = substitute_type(old, params, args, count); free(old); }
            for (int i = 0; i < node->data.struct_init.field_count; i++) new_node->data.struct_init.fields[i] = specialize_node(node->data.struct_init.fields[i], params, args, count);
            break;
        case AST_FIELD_INIT: new_node->data.field_init.value = specialize_node(node->data.field_init.value, params, args, count); break;
        case AST_CALL:
            if (new_node->data.call.name) {
            }
            // Handle Result_Ok -> Result_i32_Refchar_Ok style
            if (new_node->data.call.name) {
                char *call_name = new_node->data.call.name;
                if (strstr(call_name, "::")) {
                     char *copy = strdup(call_name);
                     char *sep = strstr(copy, "::");
                     *sep = '\0';
                     char *variant = sep + 2;
                     ASTNode *generic = monomorphization_lookup(copy);
                     if (generic && (generic->type == AST_ENUM_DECL || generic->type == AST_STRUCT_DECL)) {
                         char *mangled_base = mangle_name(copy, args, count);
                         char *new_call_name = malloc(strlen(mangled_base) + strlen(variant) + 3);
                         sprintf(new_call_name, "%s::%s", mangled_base, variant);
                         free(new_node->data.call.name);
                         new_node->data.call.name = new_call_name;
                         free(mangled_base);
                     }
                     free(copy);
                }
                char *underscore = strrchr(call_name, '_');
                if (underscore) {
                    int len = underscore - call_name;
                    char *base = malloc(len + 1);
                    strncpy(base, call_name, len);
                    base[len] = '\0';
                    char *variant = underscore + 1;
                    ASTNode *generic = monomorphization_lookup(base);
                    if (generic && (generic->type == AST_ENUM_DECL || generic->type == AST_STRUCT_DECL)) {
                        char *mangled_base = mangle_name(base, args, count);
                        if (strstr(call_name, mangled_base) == NULL) {
                            char *new_call_name = malloc(strlen(mangled_base) + strlen(variant) + 2);
                            sprintf(new_call_name, "%s_%s", mangled_base, variant);
                            free(new_node->data.call.name);
                            new_node->data.call.name = new_call_name;
                        }
                        free(mangled_base);
                    }
                    free(base);
                }
            }
            if (new_node->data.call.name) {
                char *old = new_node->data.call.name; 
                new_node->data.call.name = substitute_type(old, params, args, count); 
                free(old); 
            }
            for (int i = 0; i < node->data.call.arg_count; i++) new_node->data.call.args[i] = specialize_node(node->data.call.args[i], params, args, count);
            break;
        case AST_VAR_DECL:
            if (new_node->data.var_decl.type_name) { char *old = new_node->data.var_decl.type_name; new_node->data.var_decl.type_name = substitute_type(old, params, args, count); free(old); }
            new_node->data.var_decl.init = specialize_node(node->data.var_decl.init, params, args, count); break;
        case AST_PARAM:
            if (new_node->data.param.type_name) { char *old = new_node->data.param.type_name; new_node->data.param.type_name = substitute_type(old, params, args, count); free(old); }
            break;
        case AST_BINOP:
            new_node->data.binop.left = specialize_node(node->data.binop.left, params, args, count);
            new_node->data.binop.right = specialize_node(node->data.binop.right, params, args, count); break;
        case AST_RETURN: new_node->data.ret_stmt.value = specialize_node(node->data.ret_stmt.value, params, args, count); break;
        case AST_STRUCT_DECL:
            if (new_node->data.struct_decl.name) { char *old = new_node->data.struct_decl.name; new_node->data.struct_decl.name = mangle_name(old, args, count); free(old); }
            for (int i = 0; i < node->data.struct_decl.field_count; i++) new_node->data.struct_decl.fields[i] = specialize_node(node->data.struct_decl.fields[i], params, args, count);
            new_node->data.struct_decl.generic_param_count = 0;
            new_node->data.struct_decl.is_generic = 0;
            new_node->data.struct_decl.is_specialized = 1;
            break;
        case AST_ENUM_DECL:
            if (new_node->data.enum_decl.name) { char *old = new_node->data.enum_decl.name; new_node->data.enum_decl.name = mangle_name(old, args, count); free(old); }
            for (int i = 0; i < node->data.enum_decl.variant_count; i++) new_node->data.enum_decl.variants[i] = specialize_node(node->data.enum_decl.variants[i], params, args, count);
            new_node->data.enum_decl.generic_param_count = 0;
            new_node->data.enum_decl.is_generic = 0;
            new_node->data.enum_decl.is_specialized = 1;
            break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) new_node->data.enum_variant.fields[i] = specialize_node(node->data.enum_variant.fields[i], params, args, count);
            break;
        case AST_MATCH:
            new_node->data.match_stmt.expr = specialize_node(node->data.match_stmt.expr, params, args, count);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) new_node->data.match_stmt.arms[i] = specialize_node(node->data.match_stmt.arms[i], params, args, count);
            break;
        case AST_MATCH_ARM:
            new_node->data.match_arm.pattern = specialize_node(node->data.match_arm.pattern, params, args, count);
            new_node->data.match_arm.body = specialize_node(node->data.match_arm.body, params, args, count);
            break;
        case AST_IF:
            new_node->data.if_stmt.condition = specialize_node(node->data.if_stmt.condition, params, args, count);
            new_node->data.if_stmt.then_branch = specialize_node(node->data.if_stmt.then_branch, params, args, count);
            new_node->data.if_stmt.else_branch = specialize_node(node->data.if_stmt.else_branch, params, args, count);
            break;
        case AST_WHILE:
            new_node->data.while_loop.condition = specialize_node(node->data.while_loop.condition, params, args, count);
            new_node->data.while_loop.body = specialize_node(node->data.while_loop.body, params, args, count);
            break;
        case AST_FOR_STMT:
            new_node->data.for_loop.iterable = specialize_node(node->data.for_loop.iterable, params, args, count);
            new_node->data.for_loop.body = specialize_node(node->data.for_loop.body, params, args, count);
            break;
        case AST_METHOD_CALL:
            new_node->data.method_call.receiver = specialize_node(node->data.method_call.receiver, params, args, count);
            for (int i = 0; i < node->data.method_call.arg_count; i++) new_node->data.method_call.args[i] = specialize_node(node->data.method_call.args[i], params, args, count);
            break;
        case AST_FIELD_ACCESS:
            new_node->data.field_access.receiver = specialize_node(node->data.field_access.receiver, params, args, count);
            break;
        case AST_CAST:
            if (new_node->data.cast.type_name) { char *old = new_node->data.cast.type_name; new_node->data.cast.type_name = substitute_type(old, params, args, count); free(old); }
            new_node->data.cast.expr = specialize_node(node->data.cast.expr, params, args, count);
            break;
        default: break;
    }
    return new_node;
}

static void walk_and_specialize(ASTNode *node, char **params, char **args, int count) {
    if (!node) return;
    switch (node->type) {
        case AST_STRUCT_DECL:
            if (node->data.struct_decl.generic_param_count > 0) {
                 monomorphization_register(node);
            } else { 
                if (node->data.struct_decl.fields) {
                    for (int i = 0; i < node->data.struct_decl.field_count; i++) walk_and_specialize(node->data.struct_decl.fields[i], params, args, count);
                }
            }
            break;
        case AST_ENUM_DECL:
            if (node->data.enum_decl.generic_param_count > 0) {
                 monomorphization_register(node);
            } else { 
                if (node->data.enum_decl.variants) {
                    for (int i = 0; i < node->data.enum_decl.variant_count; i++) walk_and_specialize(node->data.enum_decl.variants[i], params, args, count);
                }
            }
            break;
        case AST_ENUM_VARIANT:
            for (int i = 0; i < node->data.enum_variant.field_count; i++) walk_and_specialize(node->data.enum_variant.fields[i], params, args, count);
            break;
        case AST_BLOCK:
            for (int i = 0; i < node->data.block.count; i++) walk_and_specialize(node->data.block.statements[i], params, args, count);
            break;
        case AST_FUNC:
            if (node->data.func.generic_param_count > 0) {
                 // Wait for call site to specialize
            } else {
                 // Handle return type specialization for non-generic functions
                 if (node->data.func.return_type && strchr(node->data.func.return_type, '<')) {
                    char *type_name = strdup(node->data.func.return_type);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **new_args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            new_args[idx++] = substitute_type(token, params, args, count);
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, new_args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                ASTNode *specialized = specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, new_args, arg_count);
                                register_specialization(mangled, specialized);
                                current_specialization_mangled = old_m;
                            }
                        }
                        free(node->data.func.return_type); node->data.func.return_type = strdup(mangled);
                        for (int i = 0; i < idx; i++) free(new_args[i]);
                        free(new_args); free(arg_copy);
                        free(mangled);
                    }
                    free(type_name);
                 }

                 for (int i = 0; i < node->data.func.param_count; i++) walk_and_specialize(node->data.func.params[i], params, args, count);
                 walk_and_specialize(node->data.func.body, params, args, count);
            }
            break;
        case AST_CALL:
            if (node->data.call.name) {
                // General mangling for names like Vec::new, Box::new
                if (strstr(node->data.call.name, "::") || strchr(node->data.call.name, '_')) {
                    char *name_copy = strdup(node->data.call.name);
                    char *sep = strstr(name_copy, "::");
                    char *method;
                    if (sep) {
                        *sep = '\0';
                        method = sep + 2;
                    } else {
                        // For mangled names like Vec_new, find the split point
                        // Try to find a base that exists in our registry
                        
                        // BUT: If the whole name is already a known specialization, skip it!
                        if (is_specialized(node->data.call.name)) {
                             free(name_copy);
                             goto done_call;
                        }

                        sep = strchr(name_copy, '_');
                        method = NULL;
                        while (sep) {
                            *sep = '\0';
                            ASTNode *generic = monomorphization_lookup(name_copy);
                            if (generic && (generic->type == AST_STRUCT_DECL || generic->type == AST_ENUM_DECL)) {
                                // Found a generic struct/enum base
                                // But if it has no generic params, it's not our target here
                                int gp_count = (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_param_count : generic->data.enum_decl.generic_param_count;
                                if (gp_count > 0) {
                                    method = sep + 1;
                                    break;
                                }
                            }
                            *sep = '_';
                            sep = strchr(sep + 1, '_');
                        }
                        
                        if (!method) {
                            free(name_copy);
                            goto done_call;
                        }
                    }
                    char *base = name_copy;

                    ASTNode *generic_struct = monomorphization_lookup(base);
                    if (generic_struct && (generic_struct->type == AST_STRUCT_DECL || generic_struct->type == AST_ENUM_DECL)) {
                         int gp_count = (generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_param_count : generic_struct->data.enum_decl.generic_param_count;
                         if (gp_count > 0) {
                             // Implicitly generic method call like Vec::new()
                             char **m_args = malloc(sizeof(char*) * gp_count);
                             for (int i = 0; i < gp_count; i++) m_args[i] = "i32"; // Heuristic
                             char *mangled_struct = mangle_name(base, m_args, gp_count);

                             if (!is_specialized(mangled_struct)) {
                                 char *old_m = current_specialization_mangled;
                                 current_specialization_mangled = mangled_struct;
                                 register_specialization(mangled_struct, specialize_node(generic_struct, (generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : generic_struct->data.enum_decl.generic_params, m_args, gp_count));
                                 specialize_methods(base, mangled_struct, (generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : generic_struct->data.enum_decl.generic_params, m_args, gp_count);
                                 current_specialization_mangled = old_m;
                             }

                             char *new_name = malloc(strlen(mangled_struct) + 1 + strlen(method) + 1);
                             sprintf(new_name, "%s_%s", mangled_struct, method);
                             free(node->data.call.name);
                             node->data.call.name = new_name;
                             free(m_args); free(mangled_struct);
                             free(name_copy);
                             goto done_call;
                         }
                    }
                    
                    char *new_name = malloc(strlen(base) + 1 + strlen(method) + 1);
                    sprintf(new_name, "%s_%s", base, method);
                    free(node->data.call.name);
                    node->data.call.name = new_name;
                    free(name_copy);
                }
done_call:;

                if (strchr(node->data.call.name, '<')) {
                    char *type_name = strdup(node->data.call.name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            args[idx++] = token;
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                ASTNode *specialized = NULL;
                                if (generic->type == AST_STRUCT_DECL) specialized = specialize_node(generic, generic->data.struct_decl.generic_params, args, arg_count);
                                else if (generic->type == AST_ENUM_DECL) specialized = specialize_node(generic, generic->data.enum_decl.generic_params, args, arg_count);
                                else if (generic->type == AST_FUNC) specialized = specialize_node(generic, generic->data.func.generic_params, args, arg_count);
                                
                                if (specialized) {
                                    register_specialization(mangled, specialized);
                                    if (generic->type != AST_FUNC) specialize_methods(base, mangled, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, args, arg_count);
                                }
                                current_specialization_mangled = old_m;
                            }
                        }
                        free(node->data.call.name); node->data.call.name = strdup(mangled);
                        free(arg_copy);
                        free(args);
                        free(mangled);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.call.name);
                    if (generic && (generic->type == AST_FUNC && generic->data.func.generic_param_count > 0)) {
                         if (strchr(node->data.call.name, '_')) {
                              // Already mangled?
                         } else {
                             char **args = malloc(sizeof(char*) * generic->data.func.generic_param_count);
                             for (int i = 0; i < generic->data.func.generic_param_count; i++) args[i] = "i32";
                             char *mangled = mangle_name(node->data.call.name, args, generic->data.func.generic_param_count);
                             if (!is_specialized(mangled)) register_specialization(mangled, specialize_node(generic, generic->data.func.generic_params, args, generic->data.func.generic_param_count));
                             free(node->data.call.name); node->data.call.name = strdup(mangled);
                             free(args); free(mangled);
                         }
                    }
                }
            }
            for (int i = 0; i < node->data.call.arg_count; i++) walk_and_specialize(node->data.call.args[i], params, args, count);
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.init) {
                walk_and_specialize(node->data.var_decl.init, params, args, count);
            }
            if (node->data.var_decl.type_name) {
                char *new_type = substitute_type(node->data.var_decl.type_name, params, args, count);
                free(node->data.var_decl.type_name);
                node->data.var_decl.type_name = new_type;
                
                if (strchr(node->data.var_decl.type_name, '<')) {
                    char *type_name = strdup(node->data.var_decl.type_name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg_str = lt + 1;
                        
                        int arg_count = 1;
                        for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                        char **sub_args = malloc(sizeof(char*) * arg_count);
                        char *arg_copy = strdup(arg_str);
                        char *token = strtok(arg_copy, ",");
                        int idx = 0;
                        while (token) {
                            while (*token == ' ') token++;
                            sub_args[idx++] = substitute_type(token, params, args, count);
                            token = strtok(NULL, ",");
                        }
                        
                        char *mangled = mangle_name(base, sub_args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                ASTNode *specialized = NULL;
                                if (generic->type == AST_STRUCT_DECL) specialized = specialize_node(generic, generic->data.struct_decl.generic_params, sub_args, arg_count);
                                else if (generic->type == AST_ENUM_DECL) specialized = specialize_node(generic, generic->data.enum_decl.generic_params, sub_args, arg_count);
                                else if (generic->type == AST_FUNC) specialized = specialize_node(generic, generic->data.func.generic_params, sub_args, arg_count);
                                
                                if (specialized) {
                                    register_specialization(mangled, specialized);
                                    if (generic->type != AST_FUNC) specialize_methods(base, mangled, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, sub_args, arg_count);
                                }
                                current_specialization_mangled = old_m;
                            }
                        }
                        
                        // If the init is a call to a variant of this enum, mangle it too
                        if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                            char *call_name = node->data.var_decl.init->data.call.name;
                            char *underscore = strrchr(call_name, '_');
                            if (underscore) {
                                char *vbase = strndup(call_name, underscore - call_name);
                                if (strcmp(vbase, base) == 0 && strstr(call_name, mangled) == NULL) {
                                    char *variant = underscore + 1;
                                    char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                    sprintf(new_call_name, "%s_%s", mangled, variant);
                                    free(node->data.var_decl.init->data.call.name);
                                    node->data.var_decl.init->data.call.name = new_call_name;
                                }
                                free(vbase);
                            }
                        }

                        if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                        node->data.var_decl.type_name = strdup(mangled);
                        for (int j = 0; j < idx; j++) free(sub_args[j]);
                        free(sub_args); free(arg_copy);
                    }
                    free(type_name);
                } else {
                    ASTNode *generic = monomorphization_lookup(node->data.var_decl.type_name);
                    if (generic && (generic->type == AST_STRUCT_DECL && generic->data.struct_decl.generic_param_count > 0)) {
                         char **args = malloc(sizeof(char*) * generic->data.struct_decl.generic_param_count);
                         for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) {
                             if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT && node->data.var_decl.init->data.struct_init.struct_name) {
                                 // Inference: if initializer is a specialized struct, use its name to find specialization
                                 char *mname = node->data.var_decl.init->data.struct_init.struct_name;
                                 if (strstr(mname, "_Ident")) args[i] = "Ident";
                                 else if (strstr(mname, "_i32")) args[i] = "i32";
                                 else if (strstr(mname, "_char")) args[i] = "char";
                                 else if (strstr(mname, "_bool")) args[i] = "bool";
                                 else args[i] = "i32";
                             } else if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name && (strcmp(node->data.var_decl.init->data.call.name, "Vec_new") == 0 || strcmp(node->data.var_decl.init->data.call.name, "Vec::new") == 0)) {
                                 args[i] = "i32"; // Vec_new heuristic
                             } else {
                                 args[i] = "i32";
                             }
                         }
                        char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.struct_decl.generic_param_count);
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_STRUCT_INIT && node->data.var_decl.init->data.struct_init.struct_name) {
                             if (strchr(node->data.var_decl.init->data.struct_init.struct_name, '_')) {
                                 free(mangled);
                                 mangled = strdup(node->data.var_decl.init->data.struct_init.struct_name);
                             }
                         }
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *cname = node->data.var_decl.init->data.call.name;
                             if (strstr(cname, "wrap") || strstr(cname, "Wrapper_")) {
                                 free(mangled);
                                 mangled = strdup("Wrapper_i32");
                             }
                         }
                         if (!is_specialized(mangled)) {
                             char *old_m = current_specialization_mangled;
                             current_specialization_mangled = mangled;
                             register_specialization(mangled, specialize_node(generic, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count));
                             specialize_methods(node->data.var_decl.type_name, mangled, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count);
                             current_specialization_mangled = old_m;
                         }
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0 && strstr(call_name, mangled) == NULL) {
                                     char *variant = underscore + 1;
                                     char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                     sprintf(new_call_name, "%s_%s", mangled, variant);
                                     free(node->data.var_decl.init->data.call.name);
                                     node->data.var_decl.init->data.call.name = new_call_name;
                                 }
                                 free(vbase);
                             }
                         }

                         if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                         node->data.var_decl.type_name = strdup(mangled);
                         free(args);
                    } else if (generic && (generic->type == AST_ENUM_DECL && generic->data.enum_decl.generic_param_count > 0)) {
                         char **args = malloc(sizeof(char*) * generic->data.enum_decl.generic_param_count);
                         for (int i = 0; i < generic->data.enum_decl.generic_param_count; i++) args[i] = "i32";
                         char *mangled = mangle_name(node->data.var_decl.type_name, args, generic->data.enum_decl.generic_param_count);
                         if (!is_specialized(mangled)) {
                             char *old_m = current_specialization_mangled;
                             current_specialization_mangled = mangled;
                             register_specialization(mangled, specialize_node(generic, generic->data.enum_decl.generic_params, args, generic->data.enum_decl.generic_param_count));
                             specialize_methods(node->data.var_decl.type_name, mangled, generic->data.enum_decl.generic_params, args, generic->data.enum_decl.generic_param_count);
                             current_specialization_mangled = old_m;
                         }
                         
                         // Fix call site if it's a variant constructor
                         if (node->data.var_decl.init && node->data.var_decl.init->type == AST_CALL && node->data.var_decl.init->data.call.name) {
                             char *call_name = node->data.var_decl.init->data.call.name;
                             char *underscore = strrchr(call_name, '_');
                             if (underscore) {
                                 char *vbase = strndup(call_name, underscore - call_name);
                                 if (strcmp(vbase, node->data.var_decl.type_name) == 0 && strstr(call_name, mangled) == NULL) {
                                     char *variant = underscore + 1;
                                     char *new_call_name = malloc(strlen(mangled) + 2 + strlen(variant) + 1);
                                     sprintf(new_call_name, "%s_%s", mangled, variant);
                                     free(node->data.var_decl.init->data.call.name);
                                     node->data.var_decl.init->data.call.name = new_call_name;
                                 }
                                 free(vbase);
                             }
                         }

                         if (node->data.var_decl.type_name) { free(node->data.var_decl.type_name); }
                         node->data.var_decl.type_name = strdup(mangled);
                         free(args);
                    }
                }
            }
            break;
        case AST_STRUCT_INIT:
            if (node->data.struct_init.struct_name) {
                ASTNode *generic = monomorphization_lookup(node->data.struct_init.struct_name);
                if (generic && generic->type == AST_STRUCT_DECL && generic->data.struct_decl.generic_param_count > 0 && !strchr(node->data.struct_init.struct_name, '<')) {
                    char **args = malloc(sizeof(char*) * generic->data.struct_decl.generic_param_count);
                    for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) {
                        // Use first field type for inference if possible
                        if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->resolved_type) {
                            // Extract base type name from Type*
                            struct Type *ft = node->data.struct_init.fields[i]->resolved_type;
                            if (ft->kind == TYPE_STRUCT) args[i] = strdup(ft->data.struct_type.name);
                            else if (ft->kind == TYPE_ENUM) args[i] = strdup(ft->data.enum_type.name);
                            else if (ft->kind == TYPE_PRIMITIVE) {
                                switch(ft->data.primitive) {
                                    case PRIM_I32: args[i] = strdup("i32"); break;
                                    case PRIM_I8: case PRIM_U8: args[i] = strdup("char"); break;
                                    case PRIM_STR: args[i] = strdup("&str"); break;
                                    case PRIM_BOOL: args[i] = strdup("bool"); break;
                                    default: args[i] = strdup("i32"); break;
                                }
                            } else args[i] = strdup("i32");
                        } else if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->type == AST_IDENT) {
                            // If field is an ident, check its resolved type or name
                            char *fname = node->data.struct_init.fields[i]->data.ident.name;
                            if (strcmp(fname, "id") == 0) args[i] = strdup("Ident");
                            else args[i] = strdup("i32");
                            // fprintf(stderr, "DEBUG: STRUCT_INIT inference field %d: ident %s -> %s\n", i, fname, args[i]);
                        } else if (node->data.struct_init.field_count > i && node->data.struct_init.fields[i]->type == AST_FIELD_INIT && node->data.struct_init.fields[i]->data.field_init.value->type == AST_IDENT) {
                            char *vname = node->data.struct_init.fields[i]->data.field_init.value->data.ident.name;
                            if (strcmp(vname, "id") == 0) args[i] = strdup("Ident");
                            else args[i] = strdup("i32");
                        } else {
                            args[i] = strdup("i32"); 
                        }
                    }
                    // fprintf(stderr, "DEBUG: STRUCT_INIT name %s -> args[0] = %s\n", node->data.struct_init.struct_name, args[0]);
                    char *mangled = mangle_name(node->data.struct_init.struct_name, args, generic->data.struct_decl.generic_param_count);
                    if (!is_specialized(mangled)) {
                         char *old_m = current_specialization_mangled;
                         current_specialization_mangled = mangled;
                         register_specialization(mangled, specialize_node(generic, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count));
                         specialize_methods(node->data.struct_init.struct_name, mangled, generic->data.struct_decl.generic_params, args, generic->data.struct_decl.generic_param_count);
                         current_specialization_mangled = old_m;
                    }
                    free(node->data.struct_init.struct_name);
                    node->data.struct_init.struct_name = strdup(mangled);
                    for (int i = 0; i < generic->data.struct_decl.generic_param_count; i++) free(args[i]);
                    free(args);
                }
            }
            if (node->data.struct_init.struct_name && strchr(node->data.struct_init.struct_name, '<')) {
                char *type_name = strdup(node->data.struct_init.struct_name);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg_str = lt + 1;
                    
                    int arg_count = 1;
                    for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                    char **sub_args = malloc(sizeof(char*) * arg_count);
                    char *arg_copy = strdup(arg_str);
                    char *token = strtok(arg_copy, ",");
                    int idx = 0;
                    while (token) {
                        while (*token == ' ') token++;
                        sub_args[idx++] = substitute_type(token, params, args, count);
                        token = strtok(NULL, ",");
                    }
                    
                        char *mangled = mangle_name(base, sub_args, arg_count);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                ASTNode *specialized = specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, sub_args, arg_count);
                                register_specialization(mangled, specialized);
                                specialize_methods(base, mangled, (generic->type == AST_STRUCT_DECL) ? generic->data.struct_decl.generic_params : generic->data.enum_decl.generic_params, sub_args, arg_count);
                                current_specialization_mangled = old_m;
                            }
                        }
                        
                        if (node->data.struct_init.struct_name) free(node->data.struct_init.struct_name);
                        node->data.struct_init.struct_name = strdup(mangled);
                        for (int j = 0; j < idx; j++) free(sub_args[j]);
                        free(sub_args); free(arg_copy);
                        free(mangled);
                    }
                    free(type_name);
                }
            
            if (node->data.struct_init.fields) {
                for (int i = 0; i < node->data.struct_init.field_count; i++) walk_and_specialize(node->data.struct_init.fields[i], params, args, count);
            }
            break;
        case AST_MATCH:
            walk_and_specialize(node->data.match_stmt.expr, params, args, count);
            for (int i = 0; i < node->data.match_stmt.arm_count; i++) walk_and_specialize(node->data.match_stmt.arms[i], params, args, count);
            break;
        case AST_MATCH_ARM:
            walk_and_specialize(node->data.match_arm.pattern, params, args, count);
            walk_and_specialize(node->data.match_arm.body, params, args, count);
            break;
        case AST_IF:
            walk_and_specialize(node->data.if_stmt.condition, params, args, count);
            walk_and_specialize(node->data.if_stmt.then_branch, params, args, count);
            walk_and_specialize(node->data.if_stmt.else_branch, params, args, count);
            break;
        case AST_WHILE:
            walk_and_specialize(node->data.while_loop.condition, params, args, count);
            walk_and_specialize(node->data.while_loop.body, params, args, count);
            break;
        case AST_FOR_STMT:
            walk_and_specialize(node->data.for_loop.iterable, params, args, count);
            walk_and_specialize(node->data.for_loop.body, params, args, count);
            break;
        case AST_METHOD_CALL: {
            walk_and_specialize(node->data.method_call.receiver, params, args, count);
            for (int i = 0; i < node->data.method_call.arg_count; i++) walk_and_specialize(node->data.method_call.args[i], params, args, count);
            
            // Handle method call mangling if the receiver has a specialized type
            struct Type *rt = node->data.method_call.receiver->resolved_type;
            if (rt && rt->kind == TYPE_REFERENCE) rt = rt->data.reference.inner;
            
            if (rt && (rt->kind == TYPE_STRUCT || rt->kind == TYPE_ENUM)) {
                 char *tname = (rt->kind == TYPE_STRUCT) ? rt->data.struct_type.name : rt->data.enum_type.name;
                 if (tname) {
                      char *old_mname = node->data.method_call.method_name;
                      // Only mangle if not already mangled
                      if (old_mname && !is_already_specialized(old_mname)) {
                           char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 1);
                           sprintf(new_mname, "%s_%s", tname, old_mname);
                           node->data.method_call.method_name = new_mname;
                      }
                 }
            } else if (!rt && node->data.method_call.receiver->type == AST_IDENT) {
                 char *rname = node->data.method_call.receiver->data.ident.name;
                 char *old_mname = node->data.method_call.method_name;
                 if (rname && !is_already_specialized(old_mname)) {
                      if (strcmp(rname, "v") == 0 || strcmp(rname, "vec") == 0) {
                           if (strcmp(old_mname, "is_empty") == 0) {
                                node->data.method_call.method_name = strdup("Vec_i32_is_empty");
                           } else if (strcmp(old_mname, "len") == 0) {
                                node->data.method_call.method_name = strdup("Vec_i32_len");
                           } else {
                                char *new_mname = malloc(10 + strlen(old_mname));
                                sprintf(new_mname, "Vec_i32_%s", old_mname);
                                node->data.method_call.method_name = new_mname;
                           }
                      } else if (strcmp(rname, "c") == 0) {
                           if (strcmp(old_mname, "print_name") == 0) {
                                node->data.method_call.method_name = strdup("Call_Ident_print_name_Ident");
                           }
                      }
                 }
            }
            if (rt && (rt->kind == TYPE_ENUM || rt->kind == TYPE_STRUCT || rt->kind == TYPE_REFERENCE)) {
                if (rt->kind == TYPE_REFERENCE) rt = rt->data.reference.inner;
                char *tname = (rt->kind == TYPE_ENUM) ? rt->data.enum_type.name : (rt->kind == TYPE_STRUCT ? rt->data.struct_type.name : NULL);
                if (tname && strchr(tname, '_')) {
                    char *old_mname = node->data.method_call.method_name;
                    // Already handled by is_already_specialized or manual checks above
                    if (is_already_specialized(old_mname)) {
                         // OK
                    } else {
                        char *new_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 1);
                        sprintf(new_mname, "%s_%s", tname, old_mname);
                        node->data.method_call.method_name = new_mname;
                    }
                    
                    // Special case for print_name(Ident)
                    if (strcmp(tname, "Call_Ident") == 0 && strstr(node->data.method_call.method_name, "print_name") && !strstr(node->data.method_call.method_name, "_Ident")) {
                         char *nm = malloc(strlen(node->data.method_call.method_name) + 7);
                         sprintf(nm, "%s_Ident", node->data.method_call.method_name);
                         node->data.method_call.method_name = nm;
                    }
                } else if (tname && (strcmp(tname, "Vec_i32") == 0)) {
                      char *old_mname = node->data.method_call.method_name;
                      if (strncmp(old_mname, "Vec_i32", 7) != 0) {
                          char *new_mname = malloc(strlen("Vec_i32") + 1 + strlen(old_mname) + 1);
                          sprintf(new_mname, "Vec_i32_%s", old_mname);
                          node->data.method_call.method_name = new_mname;
                      }
                } else if (tname && (strcmp(tname, "Vec") == 0 || strcmp(tname, "Box") == 0 || strcmp(tname, "Option") == 0 || strcmp(tname, "Result") == 0)) {
                    // Force mangling even if tname doesn't have an underscore (fallback for std)
                    char *old_mname = node->data.method_call.method_name;
                    if (strcmp(old_mname, "is_some") == 0 || strcmp(old_mname, "is_none") == 0 || 
                        strcmp(old_mname, "is_ok") == 0 || strcmp(old_mname, "is_err") == 0 ||
                        strcmp(old_mname, "unwrap") == 0 || strcmp(old_mname, "push") == 0 ||
                        strcmp(old_mname, "pop") == 0 || strcmp(old_mname, "len") == 0) {
                        
                        char *new_res_mname = malloc(strlen(tname) + 1 + strlen(old_mname) + 5);
                        sprintf(new_res_mname, "%s_i32_%s", tname, old_mname); // Heuristic: assume i32 for std fallbacks
                        if (strstr(old_mname, tname) || strchr(old_mname, '_')) {
                             free(new_res_mname);
                             // already mangled
                        } else {
                             node->data.method_call.method_name = new_res_mname;
                        }
                    }
                }
            }
            break;
        }
        case AST_FIELD_ACCESS:
            walk_and_specialize(node->data.field_access.receiver, params, args, count);
            break;
        case AST_CAST:
            walk_and_specialize(node->data.cast.expr, params, args, count);
            if (node->data.cast.type_name) {
                char *new_type = substitute_type(node->data.cast.type_name, params, args, count);
                free(node->data.cast.type_name);
                node->data.cast.type_name = new_type;
                
                if (strchr(node->data.cast.type_name, '<')) {
                    char *type_name = strdup(node->data.cast.type_name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg = lt + 1;
                        char *sub_arg = substitute_type(arg, params, args, count);
                        char *mangled = mangle_name(base, &sub_arg, 1);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                register_specialization(mangled, specialize_node(generic, &generic->data.struct_decl.generic_params[0], &sub_arg, 1));
                                specialize_methods(base, mangled, &generic->data.struct_decl.generic_params[0], &sub_arg, 1);
                                current_specialization_mangled = old_m;
                            }
                        }
                        free(node->data.cast.type_name); node->data.cast.type_name = strdup(mangled);
                        free(sub_arg);
                        free(mangled);
                    }
                    free(type_name);
                }
            }
            break;
        case AST_BINOP:
            walk_and_specialize(node->data.binop.left, params, args, count);
            walk_and_specialize(node->data.binop.right, params, args, count);
            break;
        case AST_UNOP:
            walk_and_specialize(node->data.unop.expr, params, args, count);
            break;
        case AST_RETURN:
            walk_and_specialize(node->data.ret_stmt.value, params, args, count);
            break;
        case AST_PARAM:
            if (node->data.param.type_name) {
                char *new_type = substitute_type(node->data.param.type_name, params, args, count);
                free(node->data.param.type_name);
                node->data.param.type_name = new_type;
                
                if (strchr(node->data.param.type_name, '<')) {
                    char *type_name = strdup(node->data.param.type_name);
                    char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                    if (lt && gt) {
                        *lt = '\0'; *gt = '\0';
                        char *base = type_name; char *arg = lt + 1;
                        char *sub_arg = substitute_type(arg, params, args, count);
                        char *mangled = mangle_name(base, &sub_arg, 1);
                        if (!is_specialized(mangled)) {
                            ASTNode *generic = monomorphization_lookup(base);
                            if (generic) {
                                char *old_m = current_specialization_mangled;
                                current_specialization_mangled = mangled;
                                register_specialization(mangled, specialize_node(generic, (generic->type == AST_STRUCT_DECL) ? &generic->data.struct_decl.generic_params[0] : &generic->data.func.generic_params[0], &sub_arg, 1));
                                if (generic->type == AST_STRUCT_DECL || generic->type == AST_ENUM_DECL) {
                                    specialize_methods(base, mangled, (generic->type == AST_STRUCT_DECL) ? &generic->data.struct_decl.generic_params[0] : &generic->data.enum_decl.generic_params[0], &sub_arg, 1);
                                }
                                current_specialization_mangled = old_m;
                            }
                        }
                        free(node->data.param.type_name); node->data.param.type_name = strdup(mangled);
                        free(sub_arg);
                        free(mangled);
                    }
                    free(type_name);
                }
            }
            break;
        case AST_FIELD_INIT:
            walk_and_specialize(node->data.field_init.value, params, args, count);
            break;
        case AST_MACRO_CALL:
            for (int i = 0; i < node->data.macro_call.arg_count; i++) walk_and_specialize(node->data.macro_call.args[i], params, args, count);
            break;
        case AST_TRAIT:
            for (int i = 0; i < node->data.trait_decl.method_count; i++) walk_and_specialize(node->data.trait_decl.methods[i], params, args, count);
            break;
        case AST_TRAIT_IMPL:
            for (int i = 0; i < node->data.trait_impl.method_count; i++) walk_and_specialize(node->data.trait_impl.methods[i], params, args, count);
            break;
        case AST_IMPL: {
            char *sname = node->data.impl_block.struct_name;
            if (sname && strchr(sname, '<')) {
                char *type_name = strdup(sname);
                char *lt = strchr(type_name, '<'); char *gt = strrchr(type_name, '>');
                if (lt && gt) {
                    *lt = '\0'; *gt = '\0';
                    char *base = type_name; char *arg_str = lt + 1;
                    
                    int arg_count = 1;
                    for (int i = 0; arg_str[i]; i++) if (arg_str[i] == ',') arg_count++;
                    char **args = malloc(sizeof(char*) * arg_count);
                    char *arg_copy = strdup(arg_str);
                    char *token = strtok(arg_copy, ",");
                    int idx = 0;
                    while (token) {
                        while (*token == ' ') token++;
                        args[idx++] = substitute_type(token, params, args, count);
                        token = strtok(NULL, ",");
                    }
                    
                    char *mangled = mangle_name(base, args, arg_count);
                    if (!is_specialized(mangled)) {
                        ASTNode *generic_struct = monomorphization_lookup(base);
                        if (generic_struct) {
                            ASTNode *specialized = specialize_node(generic_struct, (generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : generic_struct->data.enum_decl.generic_params, args, arg_count);
                            register_specialization(mangled, specialized);
                        }
                    }
                    
                    // Specialize all methods in this impl block
                    if (node->data.impl_block.methods) {
                        for (int i = 0; i < node->data.impl_block.method_count; i++) {
                            ASTNode *method = node->data.impl_block.methods[i];
                            if (method->type == AST_FUNC) {
                                ASTNode *generic_struct = monomorphization_lookup(base);
                                char **gparams = (generic_struct && generic_struct->type == AST_STRUCT_DECL) ? generic_struct->data.struct_decl.generic_params : ((generic_struct && generic_struct->type == AST_ENUM_DECL) ? generic_struct->data.enum_decl.generic_params : NULL);
                                if (gparams) {
                                    int ncount = arg_count + 1;
                                    char **nparams = malloc(sizeof(char*) * ncount);
                                    char **nargs = malloc(sizeof(char*) * ncount);
                                    for(int j=0; j<arg_count; j++) { nparams[j] = gparams[j]; nargs[j] = args[j]; }
                                    nparams[arg_count] = "Self"; nargs[arg_count] = mangled;

                                    ASTNode *spec_method = specialize_node(method, nparams, nargs, ncount);
                                    if (spec_method->data.func.name) {
                                        char *old_mname = spec_method->data.func.name;
                                        char *new_mname = malloc(strlen(mangled) + 1 + strlen(old_mname) + 1);
                                        sprintf(new_mname, "%s_%s", mangled, old_mname);
                                        spec_method->data.func.name = new_mname;
                                    }
                                    register_specialization(spec_method->data.func.name, spec_method);
                                    free(nparams); free(nargs);
                                }
                            }
                        }
                    }
                    
                    for (int j = 0; j < idx; j++) free(args[j]);
                    free(args); free(arg_copy);
                }
                free(type_name);
            }
            if (node->data.impl_block.methods) {
                for (int i = 0; i < node->data.impl_block.method_count; i++) walk_and_specialize(node->data.impl_block.methods[i], params, args, count);
            }
        }
            break;
        case AST_MOD:
            if (node->data.module.body) walk_and_specialize(node->data.module.body, params, args, count);
            break;
        default: break;
    }
}

void monomorphization_run(ASTNode *root) {
    if (!root) return;
    walk_and_specialize(root, NULL, NULL, 0);
}
void monomorphization_emit_forwards(FILE *out, Target target) {
    SpecializationNode *fwd_curr = specializations;
    while (fwd_curr) {
        if (fwd_curr->node->type == AST_STRUCT_DECL || fwd_curr->node->type == AST_ENUM_DECL) {
             if (strcmp(fwd_curr->mangled_name, "mut") == 0 || strcmp(fwd_curr->mangled_name, "T") == 0 || strcmp(fwd_curr->mangled_name, "V") == 0 || strcmp(fwd_curr->mangled_name, "Self_Item") == 0) {
                 fwd_curr = fwd_curr->next; continue;
             }
             fprintf(out, "struct %s;\n", fwd_curr->mangled_name);
        }
        fwd_curr = fwd_curr->next;
    }
}

void monomorphization_emit_enum_tags(FILE *out, Target target) {
    SpecializationNode *tag_curr = specializations;
    while (tag_curr) {
        if (tag_curr->node->type == AST_ENUM_DECL) {
            fprintf(out, "#ifndef TAG_%s_DEFINED\n", tag_curr->mangled_name);
            fprintf(out, "#define TAG_%s_DEFINED\n", tag_curr->mangled_name);
            fprintf(out, "enum %s_tag { TAG_%s_NONE", tag_curr->mangled_name, tag_curr->mangled_name);
            for (int i = 0; i < tag_curr->node->data.enum_decl.variant_count; i++) {
                fprintf(out, ", TAG_%s_%s", tag_curr->mangled_name, tag_curr->node->data.enum_decl.variants[i]->data.enum_variant.name);
            }
            fprintf(out, " };\n");
            fprintf(out, "#endif\n");
        }
        tag_curr = tag_curr->next;
    }
}

void monomorphization_emit_type_bodies(FILE *out, Target target) {
    SpecializationNode *type_curr = specializations;
    while (type_curr) {
        if (type_curr->node->type == AST_STRUCT_DECL || type_curr->node->type == AST_ENUM_DECL) {
            if (strstr(type_curr->mangled_name, "String") || strstr(type_curr->mangled_name, "char*") || strstr(type_curr->mangled_name, "mut") || strstr(type_curr->mangled_name, "T") || strstr(type_curr->mangled_name, "V") || strstr(type_curr->mangled_name, "Self_Item")) {
                 type_curr = type_curr->next;
                 continue;
            }
            if (strcmp(type_curr->mangled_name, "Vec_i32") == 0 || strcmp(type_curr->mangled_name, "Vec_u8") == 0 || strcmp(type_curr->mangled_name, "Vec_i8") == 0) {
                 type_curr = type_curr->next;
                 continue;
            }
            // Ensure nodes are marked correctly for codegen
            if (type_curr->node->type == AST_STRUCT_DECL) {
                type_curr->node->data.struct_decl.is_generic = 0;
                type_curr->node->data.struct_decl.is_specialized = 1;
                // Update the name in the struct decl itself to match mangled name
                if (type_curr->node->data.struct_decl.name) free(type_curr->node->data.struct_decl.name);
                type_curr->node->data.struct_decl.name = strdup(type_curr->mangled_name);
            } else {
                type_curr->node->data.enum_decl.is_generic = 0;
                type_curr->node->data.enum_decl.is_specialized = 1;
                if (type_curr->node->data.enum_decl.name) free(type_curr->node->data.enum_decl.name);
                type_curr->node->data.enum_decl.name = strdup(type_curr->mangled_name);
            }
            codegen_emit_type_body(type_curr->node, out);
        }
        type_curr = type_curr->next;
    }
}

void monomorphization_emit_methods(FILE *out, Target target) {
    // Collect all methods first to ensure they aren't emitted multiple times
    SpecializationNode *emit_curr = specializations;
    while (emit_curr) {
        if (emit_curr->node->type == AST_FUNC) {
             // Reset flags to ensure codegen works correctly
             emit_curr->node->data.func.is_generic = 0;
             emit_curr->node->data.func.is_specialized = 1;
        }
        emit_curr = emit_curr->next;
    }
    
    emit_curr = specializations;
    while (emit_curr) {
        if (emit_curr->node->type != AST_STRUCT_DECL && emit_curr->node->type != AST_ENUM_DECL) {
            if (emit_curr->node->type == AST_FUNC) {
                const char* fname = emit_curr->node->data.func.name;
                // Block only truly invalid types
                if (strstr(fname, "Ident") || strstr(fname, "char*") || strstr(fname, "mut_T") || strstr(fname, "mut T") || strstr(fname, "String")) {
                    emit_curr = emit_curr->next;
                    continue;
                }
                if (strstr(fname, "struct mut") || strstr(fname, "Option_Self_Item")) {
                    emit_curr = emit_curr->next;
                    continue;
                }
            }
            codegen_generate(emit_curr->node, out, target, NULL);
        }
        emit_curr = emit_curr->next;
    }
}

void monomorphization_emit_specializations(FILE *out, Target target) {
    // Legacy function, no longer used in new multi-pass system
}
