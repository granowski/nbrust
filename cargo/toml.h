#ifndef TOML_H
#define TOML_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOML_STRING,
    TOML_INT,
    TOML_TABLE
} TomlValueType;

typedef struct TomlValue {
    TomlValueType type;
    union {
        char *s;
        long i;
        struct TomlTable *t;
    } val;
} TomlValue;

typedef struct TomlPair {
    char *key;
    TomlValue value;
} TomlPair;

typedef struct TomlTable {
    char *name;
    int pair_count;
    TomlPair *pairs;
} TomlTable;

typedef struct TomlDoc {
    int table_count;
    TomlTable *tables;
} TomlDoc;

TomlDoc* toml_parse(const char *source);
void toml_free(TomlDoc *doc);
void toml_print(TomlDoc *doc);

#endif
