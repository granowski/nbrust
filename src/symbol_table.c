#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>

SymbolTable *symbol_table_new(SymbolTable *parent, const char *name) {
    SymbolTable *table = malloc(sizeof(SymbolTable));
    table->symbols = NULL;
    table->parent = parent;
    table->name = name ? strdup(name) : NULL;
    return table;
}

void symbol_table_free(SymbolTable *table) {
    if (!table) return;
    Symbol *s = table->symbols;
    while (s) {
        Symbol *next = s->next;
        free(s->name);
        if (s->scope) {
            symbol_table_free(s->scope);
        }
        free(s);
        s = next;
    }
    if (table->name) free(table->name);
    free(table);
}

void symbol_table_insert(SymbolTable *table, const char *name, Type *type) {
    Symbol *s = malloc(sizeof(Symbol));
    s->name = strdup(name);
    s->type = type;
    s->scope = NULL;
    s->next = table->symbols;
    table->symbols = s;
}

void symbol_table_insert_scope(SymbolTable *table, const char *name, SymbolTable *scope) {
    Symbol *s = malloc(sizeof(Symbol));
    s->name = strdup(name);
    s->type = NULL; // Scope symbols don't have a specific type themselves
    s->scope = scope;
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

Symbol *symbol_table_lookup_path(SymbolTable *table, const char *path) {
    if (path == NULL) return NULL;
    char *path_copy = strdup(path);
    SymbolTable *current_table = table;
    Symbol *result = NULL;

    char *start = path_copy;
    if (strncmp(start, "crate::", 7) == 0) {
        // Go to root
        while (current_table->parent) current_table = current_table->parent;
        start += 7;
    } else if (strncmp(start, "super::", 7) == 0) {
        if (current_table->parent) current_table = current_table->parent;
        start += 7;
    } else if (strncmp(start, "self::", 6) == 0) {
        start += 6;
    }

    char *saveptr;
    char *token = strtok_r(start, "::", &saveptr);
    while (token != NULL) {
        Symbol *s = current_table->symbols;
        result = NULL;
        while (s) {
            if (strcmp(s->name, token) == 0) {
                result = s;
                break;
            }
            s = s->next;
        }

        token = strtok_r(NULL, "::", &saveptr);
        if (token != NULL) {
            if (result && result->scope) {
                current_table = result->scope;
            } else {
                free(path_copy);
                return NULL; // Path component not found or not a scope
            }
        }
    }

    free(path_copy);
    return result;
}
