#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "types.h"

typedef struct Symbol {
    char *name;
    Type *type;
    struct SymbolTable *scope; // Non-NULL if this symbol is a module/namespace
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable {
    Symbol *symbols;
    struct SymbolTable *parent;
    char *name; // Name of the module/namespace
    int is_block;
} SymbolTable;

SymbolTable *symbol_table_new(SymbolTable *parent, const char *name);
void symbol_table_free(SymbolTable *table);
void symbol_table_insert(SymbolTable *table, const char *name, Type *type);
void symbol_table_insert_scope(SymbolTable *table, const char *name, struct SymbolTable *scope);
Symbol *symbol_table_lookup(SymbolTable *table, const char *name);
Symbol *symbol_table_lookup_path(SymbolTable *table, const char *path);

#endif
