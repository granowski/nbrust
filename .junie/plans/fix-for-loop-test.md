---
sessionId: session-260520-122826-1g5x
---

# Requirements

### Overview & Goals
The goal is to fix the compilation and execution of the `tests/for_test.rs` sample. This involves fixing several issues in the compiler's monomorphization, type checking, borrow checking, and C code generation stages that are currently preventing `Vec` and `for` loops from working correctly.

### Scope
- **In Scope**:
    - Fixing `self`/`Self` resolution in generic implementations.
    - Fixing C code generation for specialized structs and enums.
    - Fixing statement expression syntax in generated C.
    - Fixing type inference for `if` expressions and blocks.
    - Addressing borrow checker errors in the standard library.
- **Out of Scope**:
    - Implementing new language features.
    - Fixing other failing tests not related to `for_test.rs`.
    - Optimizing the generated C code beyond what's needed for correctness.


# Technical Design

### Current Implementation Issues
1.  **Monomorphization**: The `Self` type is not being consistently replaced by the specialized struct name (e.g., `Vec_i32`) in method signatures and bodies, leading to `struct self` in C.
2.  **C Codegen**: 
    - Struct initializations are sometimes emitted without the `struct` keyword or in a format C doesn't recognize as a literal.
    - Statement expressions `({ ... })` used for expression blocks are missing required semicolons.
3.  **Type Checker**: `if` expressions are failing to correctly unify branch types when blocks are involved.
4.  **Borrow Checker**: Moves are being incorrectly reported in `std` implementation, specifically around pointer manipulations in `Vec`.

### Proposed Changes
- **Monomorphization (`src/monomorphization.c`)**:
    - Enhance `substitute_type` to be more aggressive in replacing `Self`/`self`.
    - Ensure `current_specialization_mangled` is correctly set and used during all specialization passes.
- **Codegen (`src/codegen.c`)**:
    - Update `map_type` to ensure it doesn't return `struct self` when the context is known.
    - Fix `AST_STRUCT_INIT` to always emit `(struct Name){...}`.
    - In `AST_BLOCK` (as expression), ensure the last statement gets a semicolon before the closing `})`.
- **Type Checker (`src/type_checker.c`)**:
    - Update `AST_BLOCK` type resolution to return the type of the last expression.
    - Fix `AST_IF` to correctly compare branch types.
- **Borrow Checker (`src/borrow_checker.c`)**:
    - Add a check to skip or reduce strictness inside `unsafe` blocks.

### Key Decisions
- **C23 and GCC Extensions**: We continue to use C23 features (like `auto`) and GCC extensions (statement expressions) as they are already established in the codebase.
- **Monomorphization Strategy**: We will rely on explicit type substitution during monomorphization rather than trying to track context deeply in the C codegen phase.


# Summary & TODOs

### Validation Approach
I will verify the fixes by running the specific test case:
`./nbrust tests/for_test.rs -o tmp/tests/for_test-test_bin -L std`
And then running the resulting binary.

### Key Scenarios
- `Vec::new()` and `push()` work correctly without C compilation errors.
- `for x in v` correctly expands to `into_iter()` and `next()` calls.
- `println!` correctly formats the loop variable.

### TODO List for Implementation
- [ ] Fix `substitute_type` in `monomorphization.c` for `Self`.
- [ ] Fix `map_type` fallback in `codegen.c`.
- [ ] Add semicolon to final expression in `({ ... })` blocks in `codegen.c`.
- [ ] Fix block type propagation in `type_checker.c`.
- [ ] Ensure `struct` tag is present in specialized type emissions.
- [ ] Implement `unsafe` block bypass in `borrow_checker.c`.
- [ ] Verify `Vec_i32_push` signature in generated C.
- [ ] Verify `Vec_i32_new` return statement in generated C.


# Delivery Steps

###   Step 1: Fix 'struct self' and 'Self' resolution during monomorphization and codegen
Improve handling of `Self` and `self` in `src/monomorphization.c` and `src/codegen.c`.
- Update `substitute_type` in `monomorphization.c` to consistently replace `Self` and `self` with the mangled struct name.
- Ensure `map_type` in `codegen.c` always has access to the correct struct context or that types are already fully resolved/mangled before reaching it.
- Fix the `struct self` emission in function parameters.


###   Step 2: Fix missing 'struct' tags and invalid struct initialization in C output
Correct the emission of struct literals and type usages in C.
- Ensure `AST_STRUCT_INIT` always produces valid C compound literals like `(struct Name){...}`.
- Fix cases where the `struct` keyword is missing for specialized types like `Vec_i32`.
- Verify that `map_type` correctly prepends `struct` for all non-primitive, non-typedef'd types.


###   Step 3: Fix semicolon emission in statement expressions
Ensure all statements within a GCC statement expression `({ ... })` end with a semicolon.
- Update `AST_BLOCK` handling in `codegen_node_ext` to ensure the final expression in an expression block is followed by a semicolon.
- Fix the `expected ';' after expression` error seen in `realloc` calls.


###   Step 4: Fix if/else type inference and block type propagation
Resolve type mismatch errors for `if/else` expressions.
- Investigate `check_node` for `AST_IF` in `src/type_checker.c`.
- Ensure that blocks `{ ... }` correctly propagate their inner expression type instead of defaulting to `void`.
- Fix the "incompatible types i32 and void" error in `Vec::push` and similar methods.


###   Step 5: Relax borrow checker for manual memory management in std lib
Address borrow checker failures in `std/src/lib.rs`.
- Relax or skip borrow checking for `unsafe` blocks where manual memory management occurs.
- Fix specific false positives for moved values in `Vec::push` (e.g., `new_data`, `item`).
- Ensure `for_test.rs` can pass the borrow checker by correctly handling the iterator lifecycle.
