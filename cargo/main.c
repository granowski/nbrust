#include <stdio.h>
#include <stdlib.h>
#include "toml.h"

#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <build|run|test> [Cargo.toml] [--use-crates-io]\n", argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    const char *toml_path = "Cargo.toml";
    int use_crates_io = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--use-crates-io") == 0) {
            use_crates_io = 1;
        } else if (argv[i][0] != '-') {
            toml_path = argv[i];
        }
    }

    FILE *f = fopen(toml_path, "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(len + 1);
    fread(source, 1, len, f);
    source[len] = '\0';
    fclose(f);

    TomlDoc *doc = toml_parse(source);
    if (!doc) {
        fprintf(stderr, "Failed to parse %s\n", toml_path);
        free(source);
        return 1;
    }

    if (strcmp(cmd, "build") == 0 || strcmp(cmd, "run") == 0) {
        // Find project name from [package] table
        char *project_name = "output";
        for (int i = 0; i < doc->table_count; i++) {
            if (strcmp(doc->tables[i].name, "package") == 0) {
                for (int j = 0; j < doc->tables[i].pair_count; j++) {
                    if (strcmp(doc->tables[i].pairs[j].key, "name") == 0) {
                        project_name = doc->tables[i].pairs[j].value.val.s;
                    }
                }
            }
        }

        printf("Building %s...\n", project_name);

        char src_main[1024];
        snprintf(src_main, sizeof(src_main), "src/main.rs");
        if (access(src_main, F_OK) != 0) {
            snprintf(src_main, sizeof(src_main), "src/lib.rs");
        }

        // Dependency Resolution
        char *lib_args[64];
        int lib_arg_count = 0;

        for (int i = 0; i < doc->table_count; i++) {
            if (strcmp(doc->tables[i].name, "dependencies") == 0) {
                for (int j = 0; j < doc->tables[i].pair_count; j++) {
                    char *dep_name = doc->tables[i].pairs[j].key;
                    char *dep_ver = (doc->tables[i].pairs[j].value.type == TOML_STRING) ? doc->tables[i].pairs[j].value.val.s : "0.1.0";
                    
                    char dep_path[1024];
                    int found = 0;

                    // 1. Check local directory
                    snprintf(dep_path, sizeof(dep_path), "%s", dep_name);
                    if (access(dep_path, F_OK) == 0) { found = 1; }
                    
                    if (!found) {
                        snprintf(dep_path, sizeof(dep_path), "vendor/%s", dep_name);
                        if (access(dep_path, F_OK) == 0) { found = 1; }
                    }

                    // 2. Crates.io if enabled
                    if (!found && use_crates_io) {
                        printf("Downloading %s %s from crates.io...\n", dep_name, dep_ver);
                        mkdir("vendor", 0755);
                        char cmd[2048];
                        // Using curl to download and tar to extract. 
                        // Note: crates.io URL pattern: https://static.crates.io/crates/{name}/{name}-{version}.crate
                        snprintf(cmd, sizeof(cmd), "curl -L https://static.crates.io/crates/%s/%s-%s.crate -o vendor/%s-%s.tar.gz && mkdir -p vendor/%s && tar -xzf vendor/%s-%s.tar.gz -C vendor/%s --strip-components=1", 
                                 dep_name, dep_name, dep_ver, dep_name, dep_ver, dep_name, dep_name, dep_ver, dep_name);
                        if (system(cmd) == 0) {
                            snprintf(dep_path, sizeof(dep_path), "./vendor/%s", dep_name);
                            found = 1;
                        } else {
                            fprintf(stderr, "Failed to download %s from crates.io\n", dep_name);
                        }
                    }

                    if (found) {
                        char *parent_dir = strdup(dep_path);
                        char *last_slash = strrchr(parent_dir, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                        } else {
                            strcpy(parent_dir, ".");
                        }
                        lib_args[lib_arg_count] = strdup("-L");
                        lib_args[lib_arg_count+1] = parent_dir;
                        lib_arg_count += 2;
                    } else {
                        fprintf(stderr, "Warning: Dependency %s not found.\n", dep_name);
                    }
                }
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process: run nbrust
            char output_c[1024];
            snprintf(output_c, sizeof(output_c), "%s.c", project_name);
            FILE *out = fopen(output_c, "w");
            if (!out) { perror("fopen output.c"); exit(1); }
            
            dup2(fileno(out), STDOUT_FILENO);
            fclose(out);
            
            char *args[128];
            int arg_idx = 0;
            char nbrust_path[1024];
            if (access("./nbrust", F_OK) == 0) strcpy(nbrust_path, "./nbrust");
            else strcpy(nbrust_path, "../nbrust");

            args[arg_idx++] = nbrust_path;
            args[arg_idx++] = (char*)src_main;
            for (int i = 0; i < lib_arg_count; i++) {
                args[arg_idx++] = lib_args[i];
            }
            args[arg_idx++] = NULL;

            execv(args[0], args);
            perror("execv nbrust");
            exit(1);
        } else {
            int status;
            waitpid(pid, &status, 0);
            if (status != 0) {
                fprintf(stderr, "Build failed at transpilation step.\n");
                return 1;
            }

            char output_c[1024];
            snprintf(output_c, sizeof(output_c), "%s.c", project_name);
            pid = fork();
            if (pid == 0) {
                // Compile C to binary
                execlp("cc", "cc", "-o", project_name, output_c, NULL);
                perror("execl cc");
                exit(1);
            } else {
                waitpid(pid, &status, 0);
                if (status == 0) {
                    printf("Build successful: %s\n", project_name);
                    if (strcmp(cmd, "run") == 0) {
                        char exe_path[1024];
                        snprintf(exe_path, sizeof(exe_path), "./%s", project_name);
                        execl(exe_path, exe_path, NULL);
                    }
                } else {
                    fprintf(stderr, "C compilation failed.\n");
                    return 1;
                }
            }
        }
    } else {
        printf("Parsed Cargo.toml from %s:\n", toml_path);
        toml_print(doc);
    }

    toml_free(doc);
    free(source);

    return 0;
}
