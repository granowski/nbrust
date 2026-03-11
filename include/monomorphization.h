#ifndef MONOMORPHIZATION_H
#define MONOMORPHIZATION_H

#include "ast.h"

#include "codegen.h"

void monomorphization_register(ASTNode *node);
ASTNode *monomorphization_lookup(const char *name);
void monomorphization_run(ASTNode *root);
void monomorphization_emit_specializations(FILE *out, Target target);

#endif
