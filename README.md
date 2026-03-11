### nbrust: A Bare Minimum Rust-to-C Transpiler

`nbrust` is a minimal implementation of a Rust compiler written in standard C, designed to be compiled and run on NetBSD using its standard toolchain and `bsdmake`. It transpiles a subset of Rust to C23 code, allowing for efficient execution on systems with a C compiler.

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

This implementation supports a subset of Rust and transpiles it to C (C23 standard) or ARM assembly:
- **Functions & Top-Level Statements**: Support for parameters, return types, and multiple functions per file.
- **Variables**: `let` and `let mut` declarations with optional type annotations and C23 `auto` inference.
- **Control Flow**: `if`, `else`, `while`, `for`, and `return` statements.
- **Data Structures**: `struct` (unit, tuple, and named fields) and field access.
- **Enums (Sum-Types)**: Support for complex Enum variants (Unit, Tuple, Struct) with tagged union representation in the C backend.
- **Pattern Matching**: `match` expressions with support for nested tuple and struct patterns.
- **Traits & Methods**: `impl` blocks for structs and enums. Support for `trait` definitions, static dispatch, and dynamic dispatch (vtable-based) via `Box<dyn Trait>`.
- **Generics & Monomorphization**: Basic support for generic functions, structs, and enums with a specialization engine.
- **Module System**: Hierarchical modules (`mod`), path resolution (`a::b::c`), and `use` statements for name resolution.
- **Macros**: Support for `println!`, `print!`, `panic!`, `dbg!`, `matches!`, and `vec!` expansion.
- **Standard Library**: A minimal `std` stub (in `std/src/lib.rs`) providing `Box`, `Vec`, `String`, `Option`, `Result`, and `HashMap`.
- **Borrow Checking**: Initial support for move semantics and borrow rules.
- **Cargo Support**: `Cargo.toml` parsing, project orchestration, and basic crates.io dependency fetching.
- **Executable Generation**: Integrated driver to invoke system assemblers, linkers, or C compilers to produce binaries.

#### Roadmap to `rustc` & `cargo` Parity

To reach the point where `nbrust` can compile `rustc` and `cargo` themselves, the following milestones must be achieved:

1. **Borrow Checker Completeness**: Move from basic tracking to full lexical/non-lexical lifetimes (NLL) and data-flow analysis.
2. **Robust Macro Engine**: Implement a full token-based `macro_rules!` engine with hygiene and complex pattern matching.
3. **Advanced Type System**: Support for associated types, higher-rank trait bounds (HRTBs), and more comprehensive type inference.
4. **Standard Library Expansion**: Implement enough of `std` to support the compiler's own needs (I/O, collections, process management).
5. **Ownership & RAII**: Full support for `Drop` trait and deterministic destruction in the C backend.
6. **Detailed Diagnostics**: Provide high-quality error messages with spans and suggestions.
7. **Module System Refinement**: Full support for visibility rules (`pub(crate)`, etc.) and complex `use` imports.
8. **Cargo Workspace Support**: Handle multi-crate workspaces and build scripts (`build.rs`).
9. **Unsafe Rust**: Full support for raw pointers and `unsafe` blocks (currently partially supported).
