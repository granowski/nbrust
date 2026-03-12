#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SymbolTable *symbol_table_new(SymbolTable *parent, const char *name) {
    SymbolTable *table = malloc(sizeof(SymbolTable));
    table->symbols = NULL;
    table->parent = parent;
    table->name = name ? strdup(name) : NULL;
    table->is_block = 0;
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
    if (!table || !name) return;
    fprintf(stderr, "DEBUG: symbol_table_insert table=%p name=%s\n", table, name);
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
    if (!table || !name) return NULL;
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
    // Replace "::" with ":" for easier strtok
    char *p = path_copy;
    while (*p) {
        if (*p == ':' && *(p+1) == ':') {
            *p = ':';
            memmove(p+1, p+2, strlen(p+2)+1);
        }
        p++;
    }
    SymbolTable *current_table = table;
    Symbol *result = NULL;

    char *token = strtok(path_copy, ":");
    int first = 1;
    while (token != NULL) {
        if (first) {
            result = symbol_table_lookup(current_table, token);
            first = 0;
        } else {
            Symbol *s = current_table->symbols;
            result = NULL;
            while (s) {
                if (strcmp(s->name, token) == 0) {
                    result = s;
                    break;
                }
                s = s->next;
            }
        }
        
        token = strtok(NULL, ":");
        if (token != NULL) {
            if (result && result->scope) {
                current_table = result->scope;
                // Important: strtok's context might be affected by recursion or multiple strtok calls.
                // But here it's fine since it's the same string.
            } else {
                free(path_copy);
                return NULL;
            }
        }
    }

    free(path_copy);
    return result;
}
