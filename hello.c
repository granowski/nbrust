// Target: x86_64-unknown-netbsd
#include <stdio.h>

int main() {
    const char* name = "Rust";
printf("Hello, {}!", name); printf("\n");
printf("The value is: {}", 42); printf("\n");
}
