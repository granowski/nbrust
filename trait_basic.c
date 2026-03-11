// Program started
// Source read, len=239
// Source start: trait Animal {
    f
// Initializing lexer
// Initializing parser object
// Initializing parser
// current token: trait (type=39)
// next token: Animal (type=3)
// Parser initialized
struct Animal_vtable {
    int (*speak)(void* self);
};

struct Animal_object {
    void* data;
    struct Animal_vtable* vtable;
};
struct Dog {
    char* name;
};

/* Trait implementation Animal for Dog */
int Dog_Animal_speak(void* _self_ptr) {
    struct Dog* self = (struct Dog*)_self_ptr;
{
printf("Woof! My name is %s", self->name); printf("\n");
}
}
struct Animal_vtable Dog_Animal_vtable = {
    (void*)Dog_Animal_speak
};
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    struct Dog d = (struct Dog){.name = "Fido"};
Dog_Animal_speak(&d);
}
