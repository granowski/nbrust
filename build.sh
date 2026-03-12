#!/bin/sh

# Set compilation flags
CFLAGS="-O2 -Wall -Iinclude -Icargo"
CC="${CC:-cc}"

# Create object directory
mkdir -p obj

echo "Building nbrust..."
$CC $CFLAGS -c src/main.c -o obj/main.o
$CC $CFLAGS -c src/lexer.c -o obj/lexer.o
$CC $CFLAGS -c src/parser.c -o obj/parser.o
$CC $CFLAGS -c src/symbol_table.c -o obj/symbol_table.o
$CC $CFLAGS -c src/type_checker.c -o obj/type_checker.o
$CC $CFLAGS -c src/borrow_checker.c -o obj/borrow_checker.o
$CC $CFLAGS -c src/macro_expand.c -o obj/macro_expand.o
$CC $CFLAGS -c src/types.c -o obj/types.o
$CC $CFLAGS -c src/monomorphization.c -o obj/monomorphization.o
$CC $CFLAGS -c src/codegen.c -o obj/codegen.o
$CC $CFLAGS -c src/codegen_arm64.c -o obj/codegen_arm64.o
$CC $CFLAGS -c src/codegen_armv6.c -o obj/codegen_armv6.o

$CC obj/main.o obj/lexer.o obj/parser.o obj/symbol_table.o obj/type_checker.o \
    obj/borrow_checker.o obj/macro_expand.o obj/types.o obj/monomorphization.o \
    obj/codegen.o obj/codegen_arm64.o obj/codegen_armv6.o -o nbrust

if [ $? -eq 0 ]; then
    echo "nbrust built successfully."
else
    echo "Failed to build nbrust."
    exit 1
fi

echo "Building nbcargo..."
$CC $CFLAGS -c cargo/main.c -o obj/cargo_main.o
$CC $CFLAGS -c cargo/toml.c -o obj/toml.o

$CC obj/cargo_main.o obj/toml.o -o nbcargo

if [ $? -eq 0 ]; then
    echo "nbcargo built successfully."
else
    echo "Failed to build nbcargo."
    exit 1
fi

echo "Build complete."
