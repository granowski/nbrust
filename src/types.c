#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Type *type_new(TypeKind kind) {
    Type *t = malloc(sizeof(Type));
    t->kind = kind;
    return t;
}

Type *type_primitive(PrimitiveType prim) {
    Type *t = type_new(TYPE_PRIMITIVE);
    t->data.primitive = prim;
    return t;
}

Type *type_struct(const char *name) {
    Type *t = type_new(TYPE_STRUCT);
    t->data.struct_type.name = strdup(name);
    return t;
}

Type *type_enum(const char *name) {
    Type *t = type_new(TYPE_ENUM);
    t->data.enum_type.name = strdup(name);
    return t;
}

Type *type_trait(const char *name) {
    Type *t = type_new(TYPE_TRAIT);
    t->data.trait_type.name = strdup(name);
    return t;
}

Type *type_pointer(Type *inner, int is_mut) {
    Type *t = type_new(TYPE_POINTER);
    t->data.pointer.inner = inner;
    t->data.pointer.is_mut = is_mut;
    return t;
}

Type *type_reference(Type *inner, int is_mut) {
    Type *t = type_new(TYPE_REFERENCE);
    t->data.reference.inner = inner;
    t->data.reference.is_mut = is_mut;
    return t;
}

Type *type_function(Type *ret, Type **params, int count) {
    Type *t = type_new(TYPE_FUNCTION);
    t->data.function.return_type = ret;
    t->data.function.params = malloc(sizeof(Type*) * count);
    memcpy(t->data.function.params, params, sizeof(Type*) * count);
    t->data.function.param_count = count;
    return t;
}

Type *type_generic(const char *name) {
    Type *t = type_new(TYPE_GENERIC);
    t->data.generic.name = strdup(name);
    return t;
}

void type_free(Type *type) {
    if (!type) return;
    switch (type->kind) {
        case TYPE_STRUCT: free(type->data.struct_type.name); break;
        case TYPE_ENUM: free(type->data.enum_type.name); break;
        case TYPE_TRAIT: free(type->data.trait_type.name); break;
        case TYPE_POINTER: type_free(type->data.pointer.inner); break;
        case TYPE_REFERENCE: type_free(type->data.reference.inner); break;
        case TYPE_FUNCTION:
            type_free(type->data.function.return_type);
            for (int i = 0; i < type->data.function.param_count; i++) {
                type_free(type->data.function.params[i]);
            }
            free(type->data.function.params);
            break;
        case TYPE_GENERIC: free(type->data.generic.name); break;
        default: break;
    }
    free(type);
}

int type_equals(Type *t1, Type *t2) {
    if (!t1 || !t2) return t1 == t2;
    if (t1->kind != t2->kind) return 0;
    switch (t1->kind) {
        case TYPE_PRIMITIVE: return t1->data.primitive == t2->data.primitive;
        case TYPE_STRUCT: return strcmp(t1->data.struct_type.name, t2->data.struct_type.name) == 0;
        case TYPE_ENUM: return strcmp(t1->data.enum_type.name, t2->data.enum_type.name) == 0;
        case TYPE_TRAIT: return strcmp(t1->data.trait_type.name, t2->data.trait_type.name) == 0;
        case TYPE_POINTER: return t1->data.pointer.is_mut == t2->data.pointer.is_mut && type_equals(t1->data.pointer.inner, t2->data.pointer.inner);
        case TYPE_REFERENCE: return t1->data.reference.is_mut == t2->data.reference.is_mut && type_equals(t1->data.reference.inner, t2->data.reference.inner);
        case TYPE_FUNCTION:
            if (t1->data.function.param_count != t2->data.function.param_count) return 0;
            if (!type_equals(t1->data.function.return_type, t2->data.function.return_type)) return 0;
            for (int i = 0; i < t1->data.function.param_count; i++) {
                if (!type_equals(t1->data.function.params[i], t2->data.function.params[i])) return 0;
            }
            return 1;
        case TYPE_GENERIC: return strcmp(t1->data.generic.name, t2->data.generic.name) == 0;
        case TYPE_UNKNOWN: return 1;
    }
    return 0;
}

const char *type_to_string(Type *type) {
    if (!type) return "unknown";
    static char buf[1024];
    switch (type->kind) {
        case TYPE_PRIMITIVE:
            switch (type->data.primitive) {
                case PRIM_I32: return "i32";
                case PRIM_I64: return "i64";
                case PRIM_U32: return "u32";
                case PRIM_U64: return "u64";
                case PRIM_USIZE: return "usize";
                case PRIM_ISIZE: return "isize";
                case PRIM_I8: return "i8";
                case PRIM_U8: return "u8";
                case PRIM_I16: return "i16";
                case PRIM_U16: return "u16";
                case PRIM_F32: return "f32";
                case PRIM_F64: return "f64";
                case PRIM_BOOL: return "bool";
                case PRIM_STR: return "&str";
                case PRIM_VOID: return "void";
            }
            break;
        case TYPE_STRUCT: return type->data.struct_type.name;
        case TYPE_ENUM: return type->data.enum_type.name;
        case TYPE_TRAIT: return type->data.trait_type.name;
        case TYPE_POINTER:
            snprintf(buf, sizeof(buf), "*%s%s", type->data.pointer.is_mut ? "mut " : "const ", type_to_string(type->data.pointer.inner));
            return buf;
        case TYPE_REFERENCE:
            snprintf(buf, sizeof(buf), "&%s%s", type->data.reference.is_mut ? "mut " : "", type_to_string(type->data.reference.inner));
            return buf;
        case TYPE_FUNCTION:
            snprintf(buf, sizeof(buf), "fn(...) -> %s", type_to_string(type->data.function.return_type));
            return buf;
        case TYPE_GENERIC: return type->data.generic.name;
        case TYPE_UNKNOWN: return "unknown";
    }
    return "unknown";
}
