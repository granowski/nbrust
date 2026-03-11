#include "lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

void lexer_init(Lexer *l, const char *source) {
    l->source = source;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
}

static char peek(Lexer *l) {
    return l->source[l->pos];
}

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static void skip_whitespace(Lexer *l) {
    while (isspace(peek(l))) {
        advance(l);
    }
}

static Token make_token(TokenType type, const char *text, int line, int col) {
    Token t;
    t.type = type;
    t.text = strdup(text);
    t.line = line;
    t.col = col;
    return t;
}

void token_free(Token t) {
    if (t.text) free(t.text);
}

Token lexer_next_token(Lexer *l) {
    skip_whitespace(l);

    int line = l->line;
    int col = l->col;
    char c = peek(l);

    if (c == '\0') {
        return make_token(TOKEN_EOF, "EOF", line, col);
    }

    if (c == '"') {
        advance(l); // Skip opening quote
        int start = l->pos;
        while (peek(l) != '"' && peek(l) != '\0') {
            if (peek(l) == '\\') {
                advance(l); // Skip backslash
                if (peek(l) != '\0') advance(l); // Skip escaped char
            } else {
                advance(l);
            }
        }
        int len = l->pos - start;
        char *text = malloc(len + 1);
        strncpy(text, l->source + start, len);
        text[len] = '\0';
        if (peek(l) == '"') advance(l); // Skip closing quote
        
        // Unescape internal quotes for the internal representation if needed, 
        // but for now we keep the raw content between quotes.
        // Actually, codegen expects raw content and wraps it in quotes.
        
        Token t = make_token(TOKEN_STRING, text, line, col);
        free(text);
        return t;
    }

    if (isalpha(c) || c == '_') {
        int start = l->pos;
        while (isalnum(peek(l)) || peek(l) == '_') {
            advance(l);
        }
        int len = l->pos - start;
        char *text = malloc(len + 1);
        strncpy(text, l->source + start, len);
        text[len] = '\0';

        TokenType type = TOKEN_IDENT;
        if (strcmp(text, "fn") == 0) type = TOKEN_FN;
        else if (strcmp(text, "let") == 0) type = TOKEN_LET;
        else if (strcmp(text, "mut") == 0) type = TOKEN_MUT;
        else if (strcmp(text, "if") == 0) type = TOKEN_IF;
        else if (strcmp(text, "else") == 0) type = TOKEN_ELSE;
        else if (strcmp(text, "while") == 0) type = TOKEN_WHILE;
        else if (strcmp(text, "loop") == 0) type = TOKEN_LOOP;
        else if (strcmp(text, "struct") == 0) type = TOKEN_STRUCT;
        else if (strcmp(text, "impl") == 0) type = TOKEN_IMPL;
        else if (strcmp(text, "enum") == 0) type = TOKEN_ENUM;
        else if (strcmp(text, "match") == 0) type = TOKEN_MATCH;
        else if (strcmp(text, "trait") == 0) type = TOKEN_TRAIT;
        else if (strcmp(text, "for") == 0) type = TOKEN_FOR;
        else if (strcmp(text, "self") == 0) type = TOKEN_SELF_LOWER;
        else if (strcmp(text, "return") == 0) type = TOKEN_RETURN;
        else if (strcmp(text, "true") == 0) type = TOKEN_TRUE;
        else if (strcmp(text, "false") == 0) type = TOKEN_FALSE;
        else if (strcmp(text, "mod") == 0) type = TOKEN_MOD;
        else if (strcmp(text, "use") == 0) type = TOKEN_USE;
        else if (strcmp(text, "pub") == 0) type = TOKEN_PUB;
        else if (strcmp(text, "extern") == 0) type = TOKEN_EXTERN;
        else if (strcmp(text, "crate") == 0) type = TOKEN_CRATE;
        else if (strcmp(text, "unsafe") == 0) type = TOKEN_UNSAFE;
        else if (strcmp(text, "dyn") == 0) type = TOKEN_DYN;
        else if (strcmp(text, "macro_rules") == 0) type = TOKEN_MACRO_RULES;
        else if (strcmp(text, "Self") == 0) type = TOKEN_SELF_UPPER;
        else if (strcmp(text, "type") == 0) type = TOKEN_TYPE;
        else if (strcmp(text, "const") == 0) type = TOKEN_CONST;
        else if (strcmp(text, "as") == 0) type = TOKEN_AS;
        else if (strcmp(text, "_") == 0) type = TOKEN_UNDERSCORE;

        Token t = make_token(type, text, line, col);
        free(text);
        return t;
    }

    if (isdigit(c)) {
        int start = l->pos;
        while (isdigit(peek(l))) {
            advance(l);
        }
        int len = l->pos - start;
        char *text = malloc(len + 1);
        strncpy(text, l->source + start, len);
        text[len] = '\0';
        Token t = make_token(TOKEN_INT, text, line, col);
        free(text);
        return t;
    }

    advance(l);
    char text[3] = {c, '\0', '\0'};
    if (c == '/') {
        if (peek(l) == '/') {
            while (peek(l) != '\n' && peek(l) != '\0') advance(l);
            return lexer_next_token(l);
        }
        return make_token(TOKEN_SLASH, "/", line, col);
    }

    switch (c) {
        case '(': return make_token(TOKEN_LPAREN, "(", line, col);
        case ')': return make_token(TOKEN_RPAREN, ")", line, col);
        case '{': return make_token(TOKEN_LBRACE, "{", line, col);
        case '}': return make_token(TOKEN_RBRACE, "}", line, col);
        case ':':
            if (peek(l) == ':') {
                advance(l);
                return make_token(TOKEN_COLON_COLON, "::", line, col);
            }
            return make_token(TOKEN_COLON, ":", line, col);
        case ';': return make_token(TOKEN_SEMICOLON, ";", line, col);
        case ',': return make_token(TOKEN_COMMA, ",", line, col);
        case '.': return make_token(TOKEN_DOT, ".", line, col);
        case '+': return make_token(TOKEN_PLUS, "+", line, col);
        case '-':
            if (peek(l) == '>') {
                advance(l);
                return make_token(TOKEN_ARROW, "->", line, col);
            }
            return make_token(TOKEN_MINUS, "-", line, col);
        case '*': return make_token(TOKEN_STAR, "*", line, col);
        case '=':
            if (peek(l) == '=') {
                advance(l);
                return make_token(TOKEN_EQ_EQ, "==", line, col);
            } else if (peek(l) == '>') {
                advance(l);
                return make_token(TOKEN_FAT_ARROW, "=>", line, col);
            }
            return make_token(TOKEN_EQUAL, "=", line, col);
        case '!':
            if (peek(l) == '=') {
                advance(l);
                return make_token(TOKEN_BANG_EQ, "!=", line, col);
            }
            return make_token(TOKEN_BANG, "!", line, col);
        case '<':
            if (peek(l) == '=') {
                advance(l);
                return make_token(TOKEN_LT_EQ, "<=", line, col);
            }
            return make_token(TOKEN_LT, "<", line, col);
        case '>':
            if (peek(l) == '=') {
                advance(l);
                return make_token(TOKEN_GT_EQ, ">=", line, col);
            }
            return make_token(TOKEN_GT, ">", line, col);
        case '&':
            if (peek(l) == '&') {
                advance(l);
                return make_token(TOKEN_AMP_AMP, "&&", line, col);
            }
            return make_token(TOKEN_AMP, "&", line, col);
        case '|':
            if (peek(l) == '|') {
                advance(l);
                return make_token(TOKEN_BAR_BAR, "||", line, col);
            }
            return make_token(TOKEN_BAR, "|", line, col);
        case '$': return make_token(TOKEN_DOLLAR, "$", line, col);
        case '[': return make_token(TOKEN_LBRACKET, "[", line, col);
        case ']': return make_token(TOKEN_RBRACKET, "]", line, col);
    }

    return make_token(TOKEN_UNKNOWN, text, line, col);
}
