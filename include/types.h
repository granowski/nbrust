#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

typedef enum {
    TYPE_PRIMITIVE,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_TRAIT,
    TYPE_POINTER,
    TYPE_REFERENCE,
    TYPE_FUNCTION,
    TYPE_GENERIC,
    TYPE_UNKNOWN
} TypeKind;

typedef enum {
    PRIM_I32,
    PRIM_I64,
    PRIM_U32,
    PRIM_U64,
    PRIM_USIZE,
    PRIM_ISIZE,
    PRIM_I8,
    PRIM_U8,
    PRIM_I16,
    PRIM_U16,
    PRIM_F32,
    PRIM_F64,
    PRIM_BOOL,
    PRIM_STR,
    PRIM_VOID
} PrimitiveType;

typedef struct Type {
    TypeKind kind;
    union {
        PrimitiveType primitive;
        struct {
            char *name;
        } struct_type;
        struct {
            char *name;
        } enum_type;
        struct {
            char *name;
        } trait_type;
        struct {
            struct Type *inner;
            int is_mut;
        } pointer;
        struct {
            struct Type *inner;
            int is_mut;
        } reference;
        struct {
            struct Type *return_type;
            struct Type **params;
            int param_count;
        } function;
        struct {
            char *name;
        } generic;
    } data;
} Type;

Type *type_new(TypeKind kind);
Type *type_primitive(PrimitiveType prim);
Type *type_struct(const char *name);
Type *type_enum(const char *name);
Type *type_trait(const char *name);
Type *type_pointer(Type *inner, int is_mut);
Type *type_reference(Type *inner, int is_mut);
Type *type_function(Type *ret, Type **params, int count);
Type *type_generic(const char *name);
void type_free(Type *type);
int type_equals(Type *t1, Type *t2);
const char *type_to_string(Type *type);

#endif
