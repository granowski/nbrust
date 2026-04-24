#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef enum {
    TOKEN_FN,       // fn
    TOKEN_LET,      // let
    TOKEN_MUT,      // mut
    TOKEN_IDENT,    // variable names
    TOKEN_INT,      // literals
    TOKEN_LPAREN,   // (
    TOKEN_RPAREN,   // )
    TOKEN_LBRACE,   // {
    TOKEN_RBRACE,   // }
    TOKEN_RARROW,   // ->
    TOKEN_COLON,    // :
    TOKEN_COLON_COLON, // ::
    TOKEN_SEMICOLON,// ;
    TOKEN_COMMA,    // ,
    TOKEN_AMP,      // &
    TOKEN_ARROW,    // ->
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_STAR,     // *
    TOKEN_SLASH,    // /
    TOKEN_EQUAL,    // =
    TOKEN_EQ_EQ,    // ==
    TOKEN_BANG,     // !
    TOKEN_BANG_EQ,  // !=
    TOKEN_LT,       // <
    TOKEN_LT_EQ,    // <=
    TOKEN_GT,       // >
    TOKEN_GT_EQ,    // >=
    TOKEN_AMP_AMP,  // &&
    TOKEN_BAR_BAR,  // ||
    TOKEN_BAR,      // |
    TOKEN_DOT,      // .
    TOKEN_DOT_DOT,  // ..
    TOKEN_DOT_DOT_EQ, // ..=
    TOKEN_STRING,   // "string"
    TOKEN_IF,       // if
    TOKEN_IF_LET,   // if let
    TOKEN_ELSE,     // else
    TOKEN_WHILE,    // while
    TOKEN_LOOP,     // loop
    TOKEN_STRUCT,   // struct
    TOKEN_IMPL,     // impl
    TOKEN_ENUM,     // enum
    TOKEN_MATCH,    // match
    TOKEN_TRAIT,    // trait
    TOKEN_FOR,      // for
    TOKEN_IN,       // in
    TOKEN_FAT_ARROW, // =>
    TOKEN_SELF_LOWER, // self
    TOKEN_RETURN,   // return
    TOKEN_TRUE,     // true
    TOKEN_FALSE,    // false
    TOKEN_MOD,      // mod
    TOKEN_USE,      // use
    TOKEN_PUB,      // pub
    TOKEN_EXTERN,   // extern
    TOKEN_CRATE,    // crate
    TOKEN_UNSAFE,   // unsafe
    TOKEN_UNDERSCORE,// _
    TOKEN_DYN,
    TOKEN_MACRO_RULES,
    TOKEN_SELF_UPPER, // Self
    TOKEN_TYPE,       // type
    TOKEN_CONST,      // const
    TOKEN_WHERE,      // where
    TOKEN_AS,         // as
    TOKEN_DOLLAR,     // $
    TOKEN_LBRACKET,   // [
    TOKEN_RBRACKET,   // ]
    TOKEN_MOD_OP,     // %
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int line;
    int col;
} Token;

typedef struct {
    const char *source;
    int pos;
    int line;
    int col;
} Lexer;

void lexer_init(Lexer *l, const char *source);
Token lexer_next_token(Lexer *l);
void token_free(Token t);

#endif
