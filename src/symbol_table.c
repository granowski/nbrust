#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>

SymbolTable *symbol_table_new(SymbolTable *parent) {
    SymbolTable *table = malloc(sizeof(SymbolTable));
    table->symbols = NULL;
    table->parent = parent;
    return table;
}

void symbol_table_free(SymbolTable *table) {
    if (!table) return;
    Symbol *s = table->symbols;
    while (s) {
        Symbol *next = s->next;
        free(s->name);
        // type_free(s->type); // Be careful with sharing types
        free(s);
        s = next;
    }
    free(table);
}

void symbol_table_insert(SymbolTable *table, const char *name, Type *type) {
    Symbol *s = malloc(sizeof(Symbol));
    s->name = strdup(name);
    s->type = type;
    s->next = table->symbols;
    table->symbols = s;
}

Symbol *symbol_table_lookup(SymbolTable *table, const char *name) {
    SymbolTable *current = table;
    while (current) {
        Symbol *s = current->symbols;
        while (s) {
            if (strcmp(s->name, name) == 0) return s;
            s = s->next;
        }
        current = current->parent;
    }
    return NULL;
}
