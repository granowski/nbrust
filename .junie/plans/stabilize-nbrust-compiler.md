---
sessionId: session-260520-120032-whhc
---

# Requirements

### Overview & Goals
The `nbrust` compiler is currently in a broken state due to regressions introduced during the implementation of Generics and Monomorphization. Multiple tests are failing, and the standard library cannot be fully compiled. The goal is to stabilize the compiler by fixing these issues and ensuring all existing tests pass.

### Scope
- **Monomorphization**: Fix specialized node flags and parameter substitution.
- **Codegen**: Fix type mapping, qualifier handling (e.g., `mut`), and method emission.
- **Parser**: Fix syntax errors when parsing complex generic types and trait bounds.
- **Type Checker**: Resolve basic type incompatibilities in expressions.

### Acceptance Criteria
- All 49 tests in the `tests/` directory pass (or fail as expected).
- `std_test.rs` compiles and runs successfully.
- No Rust-specific generic parameters (like `T`, `Self`) appear in the generated C code.
- `nbcargo` can successfully orchestrate a build of the standard library and tests.

# Technical Design

### Current Implementation
- `src/monomorphization.c`: Clones AST nodes for specialization but fails to reset the `is_generic` flag and sometimes misses substitutions in complex types (pointers to generics).
- `src/codegen.c`: `map_type` is inconsistent in handling qualifiers like `mut` and sometimes produces invalid C syntax like `struct mut`.
- `src/parser.c`: Fails on some Rust syntax found in `std/src/lib.rs`, specifically around trait bounds and associated types.

### Key Decisions
1. **Reset `is_generic` in specialized nodes**: Specialized nodes must have `is_generic = 0` to be emitted by the codegen.
2. **Robust `substitute_type`**: This function in `monomorphization.c` must be the source of truth for all type replacements, and `map_type` in `codegen.c` should act as a final safety net.
3. **Explicit Receiver Handling**: In specialized methods, `self: &Self` must be transformed into `self: &SpecializedStruct` before codegen.

### Proposed Changes
- **`src/monomorphization.c`**:
    - Update `specialize_node` to set `is_generic = 0` for all specialized types.
    - Enhance `substitute_type` to recursively handle all type qualifiers and nested generics.
- **`src/codegen.c`**:
    - Clean up `map_type` to correctly strip qualifiers and handle pointer/reference mapping to C.
    - Ensure struct and enum definitions are correctly emitted for all specializations.
- **`src/parser.c`**:
    - Fix the parser to handle `Type: Trait<Item = T>` syntax.
    - Improve error reporting for unexpected tokens.

# Testing

### Validation Approach
- Run `./run_tests.sh` and verify that the number of passing tests increases until all pass.
- Manually inspect the generated C code for `tests/std_test.rs` to ensure no `T` or `Self` leaks.

### Key Scenarios
- **Generic Box/Vec**: Ensure `Box<i32>` and `Vec<i32>` work correctly with pointers.
- **Traits and Impls**: Ensure specialized methods on `Vec` (like `push` and `len`) are correctly generated and called.
- **Option/Result**: Ensure complex enum variants are correctly monomorphized and handled in `match` expressions.

# Delivery Steps

### ✓ Step 1: Fix Monomorphization and Specialization flags
Fix the monomorphization logic to ensure specialized nodes are correctly marked and generic parameters are fully substituted.
- Update `specialize_node` in `src/monomorphization.c` for `AST_STRUCT_DECL` and `AST_ENUM_DECL` to set `is_generic = 0` and `is_specialized = 1`.
- Fix `substitute_type` to correctly handle `Self` and `Item` substitutions in all contexts, including pointers and references.
- Ensure that `mangle_name` produces consistent names and that `monomorphization_lookup` is used to find generic definitions before specializing.

### ✓ Step 2: Stabilize Type Mapping in Codegen
Improve the type mapping logic to prevent generic types from leaking into C and handle Rust-specific qualifiers.
- Update `map_type` in `src/codegen.c` to handle `mut` and `const` qualifiers more robustly, ensuring they don't get treated as type names (e.g., preventing `struct mut`).
- Add fallback mapping for common generic names like `T`, `V`, `K` to `i32` if they somehow reach codegen, but primarily ensure they are replaced during monomorphization.
- Fix struct/enum emission to use specialized names and ensure all fields have their types mapped correctly.

### ✓ Step 3: Fix Method Specialization and Receiver Types
Ensure methods on specialized types have the correct receiver type and names.
- Fix specialized method emission to correctly identify the receiver type (the specialized struct) even when emitted outside an `impl` block.
- Update `self` and `Self` handling in `monomorphization.c` to ensure they are replaced by the specialized struct name in method signatures.
- Fix method call generation to resolve specialized method names correctly.

### * Step 4: Address Parser Regressions and Std Lib Parsing
Fix any remaining parser issues that prevent the standard library from being fully parsed.
- Investigate and fix the "Unexpected token '>'" error in `src/parser.c`, likely related to complex trait bounds or associated type syntax in `std/src/lib.rs`.
- Ensure `parse_type` and related functions handle nested generics and trait assignments correctly.