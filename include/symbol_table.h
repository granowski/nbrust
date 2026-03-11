#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "types.h"

typedef struct Symbol {
    char *name;
    Type *type;
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable {
    Symbol *symbols;
    struct SymbolTable *parent;
} SymbolTable;

SymbolTable *symbol_table_new(SymbolTable *parent);
void symbol_table_free(SymbolTable *table);
void symbol_table_insert(SymbolTable *table, const char *name, Type *type);
Symbol *symbol_table_lookup(SymbolTable *table, const char *name);

#endif
