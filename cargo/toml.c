#include "toml.h"
#include <ctype.h>

static void skip_space(const char **p) {
    while (**p && isspace(**p)) (*p)++;
}

static char *parse_key(const char **p) {
    const char *start = *p;
    while (**p && (isalnum(**p) || **p == '_' || **p == '-')) (*p)++;
    int len = *p - start;
    char *key = malloc(len + 1);
    strncpy(key, start, len);
    key[len] = '\0';
    return key;
}

static char *parse_string(const char **p) {
    if (**p != '"') return NULL;
    (*p)++;
    const char *start = *p;
    while (**p && **p != '"') (*p)++;
    int len = *p - start;
    char *s = malloc(len + 1);
    strncpy(s, start, len);
    s[len] = '\0';
    if (**p == '"') (*p)++;
    return s;
}

TomlDoc* toml_parse(const char *source) {
    TomlDoc *doc = calloc(1, sizeof(TomlDoc));
    const char *p = source;
    TomlTable *current_table = NULL;

    // Default global table
    doc->tables = malloc(sizeof(TomlTable) * 64);
    doc->table_count = 1;
    doc->tables[0].name = strdup("global");
    doc->tables[0].pairs = malloc(sizeof(TomlPair) * 64);
    doc->tables[0].pair_count = 0;
    current_table = &doc->tables[0];

    while (*p) {
        skip_space(&p);
        if (!*p) break;

        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        if (*p == '[') {
            p++;
            char *name = parse_key(&p);
            if (*p == ']') p++;
            
            doc->tables[doc->table_count].name = name;
            doc->tables[doc->table_count].pairs = malloc(sizeof(TomlPair) * 64);
            doc->tables[doc->table_count].pair_count = 0;
            current_table = &doc->tables[doc->table_count];
            doc->table_count++;
            continue;
        }

        char *key = parse_key(&p);
        skip_space(&p);
        if (*p == '=') {
            p++;
            skip_space(&p);
            TomlValue val;
            if (*p == '"') {
                val.type = TOML_STRING;
                val.val.s = parse_string(&p);
            } else if (isdigit(*p)) {
                val.type = TOML_INT;
                val.val.i = strtol(p, (char**)&p, 10);
            } else {
                // Unknown value type, skip for now
                free(key);
                while (*p && *p != '\n') p++;
                continue;
            }
            
            current_table->pairs[current_table->pair_count].key = key;
            current_table->pairs[current_table->pair_count].value = val;
            current_table->pair_count++;
        } else {
            free(key);
        }
        
        while (*p && *p != '\n') p++;
    }

    return doc;
}

void toml_free(TomlDoc *doc) {
    if (!doc) return;
    for (int i = 0; i < doc->table_count; i++) {
        free(doc->tables[i].name);
        for (int j = 0; j < doc->tables[i].pair_count; j++) {
            free(doc->tables[i].pairs[j].key);
            if (doc->tables[i].pairs[j].value.type == TOML_STRING) {
                free(doc->tables[i].pairs[j].value.val.s);
            }
        }
        free(doc->tables[i].pairs);
    }
    free(doc->tables);
    free(doc);
}

void toml_print(TomlDoc *doc) {
    for (int i = 0; i < doc->table_count; i++) {
        if (doc->tables[i].pair_count == 0 && i == 0) continue;
        printf("[%s]\n", doc->tables[i].name);
        for (int j = 0; j < doc->tables[i].pair_count; j++) {
            printf("%s = ", doc->tables[i].pairs[j].key);
            if (doc->tables[i].pairs[j].value.type == TOML_STRING) {
                printf("\"%s\"\n", doc->tables[i].pairs[j].value.val.s);
            } else {
                printf("%ld\n", doc->tables[i].pairs[j].value.val.i);
            }
        }
        printf("\n");
    }
}
