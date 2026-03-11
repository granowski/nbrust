#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdio.h>

typedef enum {
    ARCH_X86,
    ARCH_X86_64,
    ARCH_ARMV6,
    ARCH_AARCH64,
    ARCH_UNKNOWN
} Arch;

typedef enum {
    OS_MACOS,
    OS_NETBSD,
    OS_UNKNOWN
} OS;

typedef enum {
    BACKEND_C,
    BACKEND_ARM64_ASM,
    BACKEND_ARMV6_ASM
} BackendType;

typedef struct {
    Arch arch;
    OS os;
    BackendType backend;
} Target;

void codegen_generate(ASTNode *node, FILE *out, Target target, const char *crate_name);

#endif
