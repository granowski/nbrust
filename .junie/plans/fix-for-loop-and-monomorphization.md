---
sessionId: session-260519-183919-1o7s
---

# Requirements

### Overview & Goals
The objective is to fix the compilation failure in the `for_test` feature and ensure that `for` loops over `Vec` work correctly. This involves fixing parser errors, improving the monomorphization pass (especially for `self` and generic types), and making the `for` loop type checking and code generation more robust.

### Scope
- **In Scope**:
    - Parser improvements for trait associated type bounds.
    - Monomorphization fixes for `self`, `&self`, and `&mut self`.
    - Correct substitution of generic parameters during specialization.
    - Proper type checking for `for` loops (scope and type resolution).
    - Robust C code generation for `for` loops.
    - Consolidation of `Vec` implementation in the standard library.
- **Out of Scope**:
    - Implementing a full trait system (only enough to support `IntoIterator`/`Iterator` for `Vec`).
    - Fixing unrelated type checker or borrow checker issues not affecting the `for` loop feature.

# Technical Design

### Current Implementation
The compiler currently has several issues that prevent the `for_test` from working:
1. **Parser**: `parse_type_alias` in `src/parser.c` fails on `type IntoIter: Iterator<Item = Self::Item>;` because it doesn't handle the colon and complex bounds.
2. **Monomorphization**: `src/monomorphization.c` only substitutes uppercase `Self`, causing lowercase `self` parameters to be generated as `struct self` in C. Generic parameters like `T` are also sometimes missed.
3. **Type Checker**: The `AST_FOR_STMT` case in `src/type_checker.c` is a stub that doesn't resolve the loop variable type or manage scopes.
4. **Codegen**: `src/codegen.c` uses hardcoded strings for `Option` tags and makes assumptions about iterator method names.
5. **Stdlib**: `Vec` is defined inconsistently in `std/src/lib.rs` and `std/src/collections.rs`.

### Proposed Changes
#### 1. Parser Fix
Modify `parse_type_alias` to handle bounds:
- If a colon is found after the alias name, consume everything until `=` or `;`.
- Correctly handle nested generics and equality constraints within the bounds.

#### 2. Monomorphization Improvements
- Update `substitute_type` to handle `self` (lowercase) as an alias for `Self`.
- Ensure recursive substitution for pointer and reference types (e.g., `&mut self` -> `Vec_i32*`).
- Verify that all generic parameters from the parent struct/func are present in the substitution map.

#### 3. 'for' Loop Type Checking
- In `type_checker.c`, for `AST_FOR_STMT`:
    - Check the iterable's type.
    - Determine the element type (for `Vec<T>`, it's `T`).
    - Push a new scope.
    - Insert the loop variable into the scope with the element type.
    - Check the loop body.
    - Pop the scope.

#### 4. 'for' Loop Codegen
- Use the resolved type of the loop variable to generate C tags like `TAG_Option_i32_None` instead of hardcoding "i32".
- Generate the loop variable assignment using the correct `Option` variant structure.

#### 5. Standard Library
- Unify `Vec` implementation to use `data`, `len`, and `cap` fields consistently.

### File Structure
- `src/parser.c`: Update `parse_type_alias`.
- `src/monomorphization.c`: Update `substitute_type` and `specialize_node`.
- `src/type_checker.c`: Implement `AST_FOR_STMT` logic.
- `src/codegen.c`: Update `AST_FOR_STMT` codegen.
- `std/src/lib.rs` & `std/src/collections.rs`: Consolidate `Vec`.

# Delivery Steps

### ✓ Step 1: Fix Parser for Associated Type Bounds
Fix the parser to handle associated type bounds in traits.
- Modify `parse_type_alias` in `src/parser.c` to correctly parse or skip the colon and subsequent trait bounds (e.g., `: Iterator<Item = Self::Item>`).
- Ensure it doesn't consume the terminating semicolon prematurely.
- Verify that `std/src/lib.rs` can be parsed without "Unexpected token" errors.

### * Step 2: Enhance Monomorphization and 'self' substitution
Improve the monomorphization pass to correctly handle `self` and generic parameters.
- Update `substitute_type` in `src/monomorphization.c` to recognize and replace lowercase `self` with the specialized struct type.
- Ensure that pointers and references to `self` (e.g., `&mut self`) are correctly substituted during specialization.
- Fix issues where generic parameters (like `T`) remain in the specialized AST, causing C compilation errors like `unknown type name 'T'`.

###   Step 3: Implement Type Checking for 'for' loops
Implement proper type checking and scope management for `for` loops.
- Update `check_node` for `AST_FOR_STMT` in `src/type_checker.c`.
- Resolve the type of the iterable expression.
- Extract the element type (e.g., by looking at the `Item` associated type if possible, or using a heuristic for `Vec`).
- Create a new scope for the loop body and register the loop variable with its resolved type.

###   Step 4: Refactor 'for' loop Codegen
Improve code generation for `for` loops to be more robust and type-aware.
- Update `codegen_node` for `AST_FOR_STMT` in `src/codegen.c`.
- Use the resolved type of the loop variable (from the type checker) to generate the correct `Option` tag names (e.g., `TAG_Option_i32_None`).
- Remove hardcoded assumptions about `VecIter` and handle generic iterators more gracefully.
- Ensure that `.into_iter()` is only called when necessary.

###   Step 5: Consolidate Stdlib and Verify Fix
Consolidate standard library definitions and verify the fix.
- Merge the definitions of `Vec` in `std/src/lib.rs` and `std/src/collections.rs` to use consistent field names (`data` vs `ptr`).
- Run the `for_test.rs` and verify that it compiles and runs correctly.