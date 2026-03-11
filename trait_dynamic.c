// Program started
// Source read, len=284
// Source start: trait Animal {
    f
// Initializing lexer
// Initializing parser object
// Initializing parser
// current token: trait (type=39)
// next token: Animal (type=3)
// Parser initialized
// Top level: current=trait (type=39), next=Animal (type=3)
struct Animal_vtable {
    int (*speak)(void* self);
};

struct Animal_object {
    void* data;
    struct Animal_vtable* vtable;
};
// Top level: current=struct (type=35), next=Dog (type=3)
struct Dog {
    struct char* name;
};

// Top level: current=impl (type=36), next=Animal (type=3)
/* Trait implementation Animal for Dog */
int Dog_Animal_speak(void* _self_ptr) {
    struct Dog* self = (struct Dog*)_self_ptr;
{
printf("Woof! My name is {}", self->name); printf("\n");
}
}
struct Animal_vtable Dog_Animal_vtable = {
    (void*)Dog_Animal_speak
};
// Top level: current=fn (type=0), next=main (type=3)
// Target: x86_64-unknown-netbsd
#include <stdio.h>

int main() {
    struct Dog d = (struct Dog){.name = "Fido"};
    struct Animal_object animal_obj = (struct Animal_object){ .data = (void*)&d, .vtable = &Dog_Animal_vtable };
animal_obj.vtable->speak(animal_obj.data);
}
