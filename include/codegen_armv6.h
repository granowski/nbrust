#ifndef CODEGEN_ARMV6_H
#define CODEGEN_ARMV6_H

#include "ast.h"
#include "codegen.h"
#include <stdio.h>

void codegen_armv6_generate(ASTNode *node, FILE *out, Target target);

#endif
