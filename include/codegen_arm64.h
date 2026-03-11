#ifndef CODEGEN_ARM64_H
#define CODEGEN_ARM64_H

#include "ast.h"
#include "codegen.h"
#include <stdio.h>

void codegen_arm64_generate(ASTNode *node, FILE *out, Target target);

#endif
