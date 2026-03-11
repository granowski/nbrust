#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "type_checker.h"
#include "macro_expand.h"
#include "borrow_checker.h"
#include "monomorphization.h"

#include <string.h>

#include <libgen.h>

static char *lib_paths[64];
static int lib_path_count = 0;

static void process_file(const char *path, Target target);
static char *output_filename = NULL;
static int compile_only = 0;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.rs> [-L <lib_path>] [-o <output>] [-c] [--arch <x86|x86_64|armv6|aarch64>] [--os <macos|netbsd>] [--backend <c|arm64-asm|armv6-asm>]\n", argv[0]);
        return 1;
    }

    char *source_path = NULL;
    Target target = { ARCH_X86_64, OS_NETBSD, BACKEND_C }; // Default target

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            lib_paths[lib_path_count++] = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_only = 1;
        } else if (strcmp(argv[i], "--arch") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "x86") == 0) target.arch = ARCH_X86;
            else if (strcmp(argv[i+1], "x86_64") == 0) target.arch = ARCH_X86_64;
            else if (strcmp(argv[i+1], "armv6") == 0) target.arch = ARCH_ARMV6;
            else if (strcmp(argv[i+1], "aarch64") == 0) target.arch = ARCH_AARCH64;
            i++;
        } else if (strcmp(argv[i], "--os") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "macos") == 0) target.os = OS_MACOS;
            else if (strcmp(argv[i+1], "netbsd") == 0) target.os = OS_NETBSD;
            i++;
        } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            if (strcmp(argv[i+1], "c") == 0) target.backend = BACKEND_C;
            else if (strcmp(argv[i+1], "arm64-asm") == 0) target.backend = BACKEND_ARM64_ASM;
            else if (strcmp(argv[i+1], "armv6-asm") == 0) {
                target.backend = BACKEND_ARMV6_ASM;
                target.arch = ARCH_ARMV6;
            }
            i++;
        } else if (source_path == NULL) {
            source_path = argv[i];
        }
    }

    if (!source_path) {
        fprintf(stderr, "Error: No source file specified.\n");
        return 1;
    }

    if (output_filename) {
        char cmd[2048];
        char tmp_source[1024];
        if (target.backend == BACKEND_C) {
            snprintf(tmp_source, sizeof(tmp_source), "/tmp/nbrust_out.c");
        } else {
            snprintf(tmp_source, sizeof(tmp_source), "/tmp/nbrust_out.s");
        }

        FILE *f = fopen(tmp_source, "w");
        if (!f) { perror("fopen tmp"); return 1; }
        
        // Temporarily redirect stdout to our tmp file
        // This is a bit hacky, better would be to pass FILE* to process_file
        // But for now, we'll use freopen
        FILE *original_stdout = stdout;
        stdout = f;
        process_file(source_path, target);
        fclose(f);
        stdout = original_stdout;

        if (!compile_only) {
            if (target.backend == BACKEND_C) {
                snprintf(cmd, sizeof(cmd), "cc -std=c2x %s -o %s", tmp_source, output_filename);
            } else if (target.backend == BACKEND_ARM64_ASM) {
                if (target.os == OS_MACOS) {
                    snprintf(cmd, sizeof(cmd), "as -arch arm64 %s -o /tmp/nbrust_out.o && ld -o %s /tmp/nbrust_out.o -lSystem", tmp_source, output_filename);
                } else {
                    snprintf(cmd, sizeof(cmd), "as %s -o /tmp/nbrust_out.o && ld -o %s /tmp/nbrust_out.o", tmp_source, output_filename);
                }
            } else if (target.backend == BACKEND_ARMV6_ASM) {
                snprintf(cmd, sizeof(cmd), "as %s -o /tmp/nbrust_out.o && ld -o %s /tmp/nbrust_out.o", tmp_source, output_filename);
            }
            printf("Running: %s\n", cmd);
            int res = system(cmd);
            if (res != 0) {
                fprintf(stderr, "Compilation failed with code %d\n", res);
                return 1;
            }
        } else {
             snprintf(cmd, sizeof(cmd), "cp %s %s", tmp_source, output_filename);
             system(cmd);
        }
    } else {
        process_file(source_path, target);
    }

    return 0;
}

static char *current_crate_name = NULL;

static void process_file(const char *path, Target target) {
    char *old_crate = current_crate_name;
    if (strstr(path, "/src/lib.rs")) {
        char *p = strdup(path);
        char *lib_rs = strstr(p, "/src/lib.rs");
        *lib_rs = '\0';
        char *last_slash = strrchr(p, '/');
        if (last_slash) {
            current_crate_name = strdup(last_slash + 1);
        } else {
            current_crate_name = strdup(p);
        }
        free(p);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(len + 1);
    if (!source) { perror("malloc"); return; }
    fread(source, 1, len, f);
    source[len] = '\0';
    fclose(f);

    Lexer l;
    lexer_init(&l, source);

    Parser p;
    parser_init(&p, &l);
    
    // Check if we should auto-include prelude
    static int prelude_included = 0;
    if (!prelude_included && strstr(path, "std/") == NULL && !strstr(path, "prelude.rs")) {
         prelude_included = 1;
         // process_file("std/src/prelude.rs", target); 
    }

    while (p.current.type != TOKEN_EOF) {
        if (p.current.type == TOKEN_PUB) {
            consume(&p, TOKEN_PUB);
        }
        ASTNode *ast = NULL;
        if (p.current.type == TOKEN_STRUCT) {
            ast = parse_struct(&p);
        } else if (p.current.type == TOKEN_IMPL) {
            ast = parse_impl(&p);
        } else if (p.current.type == TOKEN_TRAIT) {
            ast = parse_trait(&p);
        } else if (p.current.type == TOKEN_USE) {
            ast = parse_use(&p);
        } else if (p.current.type == TOKEN_MOD) {
            ast = parse_mod(&p);
            if (ast && ast->type == AST_MOD && ast->data.module.body == NULL) {
                // External module: mod name;
                char mod_path[1024];
                char *dir = dirname(strdup(path));
                snprintf(mod_path, sizeof(mod_path), "%s/%s.rs", dir, ast->data.module.name);
                
                FILE *mf = fopen(mod_path, "r");
                if (!mf) {
                    snprintf(mod_path, sizeof(mod_path), "%s/%s/mod.rs", dir, ast->data.module.name);
                    mf = fopen(mod_path, "r");
                }
                
                if (mf) {
                    fclose(mf);
                    process_file(mod_path, target);
                }
                free(dir);
            }
        } else if (p.current.type == TOKEN_ENUM) {
            ast = parse_enum(&p);
        } else if (p.current.type == TOKEN_TYPE) {
            ast = parse_type_alias(&p);
        } else if (p.current.type == TOKEN_CONST) {
            ast = parse_const(&p);
        } else if (p.current.type == TOKEN_MACRO_RULES) {
            ast = parse_macro_rules(&p);
        } else if (p.current.type == TOKEN_FN) {
            ast = parse_function(&p);
        } else if (p.current.type == TOKEN_EXTERN) {
            if (p.next.type == TOKEN_CRATE) {
                ast = parse_extern_crate(&p);
                if (ast && ast->type == AST_EXTERN_CRATE) {
                     if (strcmp(ast->data.extern_crate.name, "std") == 0) {
                        // Special case: extern crate std;
                        process_file("std/src/lib.rs", target);
                     } else {
                        // Search in library paths
                        int found = 0;
                        for (int i = 0; i < lib_path_count; i++) {
                            char crate_path[1024];
                            snprintf(crate_path, sizeof(crate_path), "%s/%s/src/lib.rs", lib_paths[i], ast->data.extern_crate.name);
                            FILE *cf = fopen(crate_path, "r");
                            if (cf) {
                                fclose(cf);
                                process_file(crate_path, target);
                                found = 1;
                                break;
                            }
                            // Also try <lib_path>/<crate_name>.rs
                            snprintf(crate_path, sizeof(crate_path), "%s/%s.rs", lib_paths[i], ast->data.extern_crate.name);
                            cf = fopen(crate_path, "r");
                            if (cf) {
                                fclose(cf);
                                process_file(crate_path, target);
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            fprintf(stderr, "Error: Could not find crate '%s' in any library path.\n", ast->data.extern_crate.name);
                        }
                     }
                }
            } else {
                ast = parse_extern_block(&p);
            }
        } else if (p.current.type == TOKEN_SEMICOLON) {
            consume(&p, TOKEN_SEMICOLON);
            continue;
        } else if (p.current.type == TOKEN_IDENT || p.current.type == TOKEN_LET || p.current.type == TOKEN_MATCH || p.current.type == TOKEN_IF || p.current.type == TOKEN_LBRACE) {
            // These are probably part of a function or some other top-level item that failed to parse correctly
            // or we are inside a context that allows statements.
            // For now, let's try to parse it as a statement if we are not at top level, 
            // but the current structure of main.c assumes top-level items.
            ast = parse_statement(&p);
        } else {
            // Skip unknown tokens at top level
            lexer_next_token(p.lexer); 
            p.current = p.next;
            p.next = lexer_next_token(p.lexer);
            continue;
        }
        
        if (ast) {
            int is_generic = (ast->type == AST_FUNC && ast->data.func.generic_param_count > 0) ||
                             (ast->type == AST_STRUCT_DECL && ast->data.struct_decl.generic_param_count > 0) ||
                             (ast->type == AST_ENUM_DECL && ast->data.enum_decl.generic_param_count > 0);

            if (is_generic) {
                monomorphization_register(ast);
            }
            
            macro_expand_run(ast);
            monomorphization_run(ast);
            
            // type_checker_run(ast);
            // borrow_checker_run(ast);
            
            // Check if we are in the main file and at the end of it
            static int once = 0;
            if (!once) {
                 printf("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n");
                 once = 1;
            }
            if (!is_generic) {
                codegen_generate(ast, stdout, target, current_crate_name);
                if (ast->type == AST_BINOP || ast->type == AST_IDENT || ast->type == AST_LITERAL || ast->type == AST_CALL || ast->type == AST_METHOD_CALL || ast->type == AST_MACRO_CALL || ast->type == AST_FIELD_ACCESS || ast->type == AST_UNOP || ast->type == AST_IF || ast->type == AST_MATCH || ast->type == AST_BLOCK) {
                    printf(";\n");
                }
            }
            ast_free(ast);
        }
    }
    free(source);
    current_crate_name = old_crate;
}
