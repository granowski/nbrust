### nbrust: A Bare Minimum Rust-to-C Transpiler

`nbrust` is a minimal implementation of a Rust compiler written in standard C, designed to be compiled and run on NetBSD using its standard toolchain and `bsdmake`.

#### Architecture

- **Lexer (`src/lexer.c`, `include/lexer.h`)**: Converts Rust source code into a stream of tokens. It handles keywords (`fn`, `let`, `mut`), identifiers, integers, and basic operators.
- **Parser (`src/parser.c`, `include/parser.h`, `include/ast.h`)**: Performs recursive descent parsing to build an Abstract Syntax Tree (AST). It currently supports function definitions, blocks, `let` variable declarations, and simple binary expressions.
- **Codegen (`src/codegen.c`, `include/codegen.h`)**: Transpiles the AST into equivalent C code. It handles `main()` specifically to include `<stdio.h>` and return `int`.
- **Main (`src/main.c`)**: Entry point that reads the source file, initializes the lexer and parser, and triggers code generation.
- **Cargo Tool (`cargo/`)**: A separate tool (`nbcargo`) with its own TOML parser to handle `Cargo.toml` files, isolated from the compiler core.

#### Building

To build the project on NetBSD:
```bash
make
```
To clean the build artifacts:
```bash
make clean
```

#### Usage

To compile a Rust source file (e.g., `tests/simple.rs`):
```bash
./nbrust tests/simple.rs [--arch <x86|x86_64|armv6|aarch64>] [--os <macos|netbsd>] [--backend <c|arm64-asm>]
```
This will output the transpiled C code or ARM64 assembly to `stdout`. You can specify the target architecture, operating system, and backend. Default is `x86_64`, `netbsd`, and `c` backend.

To parse a `Cargo.toml` file:
```bash
./nbcargo tests/Cargo.toml
```

You can redirect this to a `.c` or `.s` file and compile it:
```bash
./nbrust tests/simple.rs > output.c
cc -o output output.c
./output
```
Or for ARM64 assembly:
```bash
./nbrust tests/arm64_basic.rs --backend arm64-asm > output.s
as -o output.o output.s
ld -o output output.o
./output
```
(Note: Linker commands may vary by OS; on macOS use `cc` to link assembly).

#### Current Capabilities

This implementation supports a subset of Rust and transpiles it to C or ARM64 assembly:
- **Functions**: Support for parameters, return types, and multiple functions per file.
- **Variables**: `let` and `let mut` declarations with optional type annotations.
- **Control Flow**: `if`, `else`, `while`, and `return` statements (C and ARM64 backend).
- **Data Structures**: `struct` definitions and field access (C backend).
- **Methods**: `impl` blocks with method support (C backend).
- **Macros**: Basic support for `println!` and `print!` (C backend), mapped to `printf`.
- **Strings**: String literal support (`&str` mapped to `const char*` in C).
- **Expressions**: Arithmetic, logical (`&&`, `||`), and comparison operators.
- **Boolean Literals**: `true` and `false`.
- **ARM64 Assembly Backend**: Supports function definitions (with parameters), function calls, local variables, integer arithmetic, comparison operators, control flow (`if`, `while`), string literals, `println!`, `match`, and returns.
- **Cargo Support**: Basic parsing of `Cargo.toml` files via the separate `nbcargo` tool.

#### Current Limitations & Next Steps

- **Standard Library**: No `std` implementation.
- **Error Handling**: Basic error messages, no detailed diagnostics.
- **Pointers & References**: Only basic `&self` support, no general references.
- **Cargo**: Basic `Cargo.toml` parsing implemented.
