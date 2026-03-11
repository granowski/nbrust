### nbrust: A Bare Minimum Rust-to-C Transpiler

`nbrust` is a minimal implementation of a Rust compiler written in standard C, designed to be compiled and run on NetBSD using its standard toolchain and `bsdmake`.

#### Architecture

- **Lexer (`src/lexer.c`, `include/lexer.h`)**: Converts Rust source code into a stream of tokens. It handles keywords (`fn`, `let`, `mut`, `enum`, `match`, `trait`, `impl`, `type`), identifiers, integers, and string literals.
- **Parser (`src/parser.c`, `include/parser.h`, `include/ast.h`)**: Performs recursive descent parsing to build an Abstract Syntax Tree (AST). It supports function definitions, blocks, `let` variable declarations, `if`/`else`, `while`, `enum` with complex variants, `struct`, `impl`, `trait`, `match` expressions, and top-level statements.
- **Type Checker & Monomorphization (`src/type_checker.c`, `src/monomorphization.c`)**: Performs basic type checking and specializes generic functions and types (structs/enums) before code generation.
- **Codegen (`src/codegen.c`, `src/codegen_arm64.c`, `src/codegen_armv6.c`)**: Transpiles the AST into equivalent C23 code or ARM assembly. The C backend uses tagged unions for Enums and `#define` macros for pattern matching field extraction.
- **Main (`src/main.c`)**: Entry point that manages the compilation pipeline, including executable generation via system tools (as, ld, cc).
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

To compile a Rust source file into an executable (e.g., `tests/trait_basic.rs`):
```bash
./nbrust tests/trait_basic.rs -o trait_test --os macos --backend c
./trait_test
```

Options:
- `-o <file>`: Output executable name.
- `-c`: Compile only (do not link).
- `--arch <x86|x86_64|armv6|aarch64>`: Target architecture (default: `x86_64`).
- `--os <macos|netbsd>`: Target OS (default: `netbsd`).
- `--backend <c|arm64-asm|armv6-asm>`: Backend to use (default: `c`).

To parse a `Cargo.toml` file:
```bash
./nbcargo tests/Cargo.toml
```

You can still redirect output to a `.c` or `.s` file if needed:
```bash
./nbrust tests/simple.rs > output.c
cc -std=c2x -o output output.c
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

This implementation supports a subset of Rust and transpiles it to C (C23 standard) or ARM assembly:
- **Functions & Top-Level Statements**: Support for parameters, return types, and multiple functions per file. `let` and `match` are supported at the top level.
- **Variables**: `let` and `let mut` declarations with optional type annotations and C23 `auto` inference.
- **Control Flow**: `if`, `else`, `while`, and `return` statements.
- **Data Structures**: `struct` (unit, tuple, and named fields) and field access.
- **Enums (Sum-Types)**: Support for complex Enum variants (Unit, Tuple, Struct) with tagged union representation in the C backend.
- **Pattern Matching**: `match` expressions with support for nested tuple and struct patterns.
- **Traits & Methods**: `impl` blocks for structs and enums. Support for `trait` definitions, static dispatch, and dynamic dispatch (vtable-based) via `Box<dyn Trait>`.
- **Generics & Monomorphization**: Basic support for generic functions, structs, and enums with a specialization engine.
- **Module System**: Hierarchical modules (`mod`), path resolution (`a::b::c`), and basic `use` statements for cross-module name resolution.
- **Macros**: Support for `println!`, `print!`, and `panic!` with format string interpolation.
- **Strings**: String literal support (`&str` mapped to `const char*` in C).
- **Expressions**: Arithmetic, logical (`&&`, `||`), and comparison operators.
- **Boolean Literals**: `true` and `false`.
- **Assembly Backends**: ARM64 and ARMv6 backends support function definitions, local variables, integer arithmetic, control flow, string literals, and basic `match`.
- **Cargo Support**: Basic parsing of `Cargo.toml` files via the separate `nbcargo` tool.
- **Executable Generation**: Integrated driver to invoke system assemblers, linkers, or C compilers to produce runnable binaries.

#### Current Limitations & Next Steps

- **Standard Library**: No `std` implementation (beyond minimal macro stubs).
- **Type Checking**: Basic type checking in progress; some dispatch still relies on naming heuristics.
- **Error Handling**: Basic error messages, no detailed diagnostics or borrow checker.
- **Ownership & Borrows**: No borrow checker yet; memory management is primarily static or relies on the C backend's behavior.
- **Cargo**: Basic `Cargo.toml` parsing only; not yet integrated into the build process.
