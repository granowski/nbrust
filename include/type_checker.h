#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include "ast.h"
#include "symbol_table.h"

void type_checker_run(ASTNode *root);

#endif
