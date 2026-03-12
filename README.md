### nbrust: A Bare Minimum Rust-to-C Transpiler

`nbrust` is a minimal implementation of a Rust compiler written in standard C, designed to be compiled and run on NetBSD using its standard toolchain and `bsdmake`. It transpiles a subset of Rust to C23 code, allowing for efficient execution on systems with a C compiler.

> [!NOTE]
> This project was developed as a technical demonstration. 100% of the code in this repository was generated using the [**Junie CLI**](https://junie.jetbrains.com) powered by the [**Gemini 3 Flash**](https://deepmind.google/technologies/gemini/flash/) model.

#### Architecture

- **Lexer (`src/lexer.c`, `include/lexer.h`)**: Converts Rust source code into a stream of tokens. It handles keywords (`fn`, `let`, `mut`, `enum`, `match`, `trait`, `impl`, `type`), identifiers, integers, and string literals.
- **Parser (`src/parser.c`, `include/parser.h`, `include/ast.h`)**: Performs recursive descent parsing to build an Abstract Syntax Tree (AST). It supports function definitions, blocks, `let` variable declarations, `if`/`else`, `while`, `enum` with complex variants, `struct`, `impl`, `trait`, `match` expressions, and top-level statements.
- **Macro Expansion (`src/macro_expand.c`)**: A preliminary expansion pass that handles `macro_rules!` (basic text-based), built-in macros like `println!`, `panic!`, `dbg!`, `matches!`, and a hardcoded expansion for `vec!`.
- **Monomorphization (`src/monomorphization.c`)**: Specializes generic functions, structs, and enums into concrete types before code generation, allowing for static dispatch and type-safe generic code.
- **Type Checker (`src/type_checker.c`)**: Performs basic type checking using a hierarchical symbol table. It resolves types for variables, functions, and struct/enum fields.
- **Borrow Checker (`src/borrow_checker.c`)**: A skeleton implementation of Rust's ownership system. It tracks variable moves and basic immutable/mutable borrows to prevent common memory safety issues at compile time.
- **Codegen (`src/codegen.c`, `src/codegen_arm64.c`, `src/codegen_armv6.c`)**: Transpiles the AST into equivalent C23 code or ARM assembly. The C backend uses tagged unions for Enums and `#define` macros for pattern matching field extraction.
- **Main (`src/main.c`)**: Entry point that manages the compilation pipeline: lexing -> parsing -> macro expansion -> monomorphization -> type checking -> borrow checking -> codegen.
- **Cargo Tool (`cargo/`)**: A separate tool (`nbcargo`) with its own TOML parser to handle `Cargo.toml` files and orchestrate builds, including basic dependency fetching from crates.io.

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

To build and run a project using `nbcargo`:
```bash
./nbcargo run Cargo.toml
```

#### Current Capabilities

This implementation supports a significant subset of Rust and transpiles it to C (C23 standard) or ARM assembly (AArch64/ARMv6):
- **Backends**: C, AArch64 (ARM64) Assembly, and ARMv6 Assembly.
- **Targets**: macOS and NetBSD (ELF).
- **Functions & Top-Level Statements**: Support for parameters, return types, and multiple functions per file.
- **Variables**: `let` and `let mut` declarations with optional type annotations and C23 `auto` inference.
- **Control Flow**: `if`, `else`, `while`, `for`, and `return` statements.
- **Data Structures**: `struct` (unit, tuple, and named fields) and field access.
- **Enums (Sum-Types)**: Support for complex Enum variants (Unit, Tuple, Struct) with tagged union representation in the C backend.
- **Pattern Matching**: `match` expressions with support for nested tuple and struct patterns.
- **Traits & Methods**: `impl` blocks for structs and enums. Support for `trait` definitions, static dispatch, and dynamic dispatch (vtable-based) via `Box<dyn Trait>`.
- **Advanced Type System**: Support for generics, monomorphization, and associated types, critical for trait completeness.
- **Module System**: Hierarchical modules (`mod`), path resolution (`a::b::c`), and `use` statements for name resolution.
- **Macro Engine**: Support for `macro_rules!` (structured expansion via re-parsing), and built-ins like `println!`, `print!`, `panic!`, `dbg!`, `matches!`, and `vec!`.
- **Borrow Checker**: Scope-aware lifetime management, move-after-borrow detection, and end-of-block borrow release.
- **Ownership & RAII**: Support for deterministic destruction hooks in the C backend, laying the foundation for the `Drop` trait.
- **Unsafe Rust**: Support for raw pointer operations and `unsafe` blocks.
- **Standard Library**: An expanded `std` stub (in `std/src/lib.rs`) providing `Box`, `Vec`, `String`, `Option`, `Result`, `HashMap`, and new `io` and `process` modules.
- **Cargo Tool (`nbcargo`)**: `Cargo.toml` parsing, dependency resolution, and project orchestration.
- **Executable Generation**: Integrated driver to invoke system assemblers, linkers, or C compilers to produce binaries.

#### Roadmap to NetBSD Self-Hosting

With core feature parity achieved, the next milestones focus on stabilization and self-hosting:

1. **Self-Hosting Verification**: Compiling `nbrust` and `nbcargo` using themselves on NetBSD.
2. **Performance Optimization**: Reducing memory footprint and improving compilation speed.
3. **Advanced Lifetime Analysis**: Moving towards full non-lexical lifetimes (NLL).
4. **Diagnostic Improvements**: Enhancing error reporting with more detailed spans and recovery.
5. **Standard Library Completeness**: Implementing more of `std` as required by more complex crates.
6. **Cargo Workspace Support**: Improving handling for multi-crate workspaces and build scripts.
