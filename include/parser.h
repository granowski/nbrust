#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer *lexer;
    Token current;
    Token next;
} Parser;

static ASTNode *parse_single_pattern_internal(Parser *p);

void parser_init(Parser *p, Lexer *l);
Token parser_peek(Parser *p);
void real_consume(Parser *p, TokenType type, int call_line, const char *type_name);
#define consume(p, type) real_consume(p, type, __LINE__, #type)
ASTNode *parse_function(Parser *p);
ASTNode *parse_struct(Parser *p);
ASTNode *parse_impl(Parser *p);
ASTNode *parse_enum(Parser *p);
ASTNode *parse_statement(Parser *p);
ASTNode *parse_expression(Parser *p);
ASTNode *parse_trait(Parser *p);
ASTNode *parse_mod(Parser *p);
ASTNode *parse_use(Parser *p);
ASTNode *parse_extern_block(Parser *p);
ASTNode *parse_extern_crate(Parser *p);
ASTNode *parse_macro_rules(Parser *p);
ASTNode *parse_type_alias(Parser *p);
ASTNode *parse_const(Parser *p);
ASTNode *parse_block(Parser *p);

#endif
