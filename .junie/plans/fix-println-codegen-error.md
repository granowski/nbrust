---
sessionId: session-260519-182237-1fun
---

# Requirements

### Overview & Goals
The goal is to fix a compilation error in the `asm_test` suite where the use of `println` as a function (without the `!`) results in undeclared identifier errors in the generated C code. We will also improve the compiler's robustness by supporting `println` and `print` as both macros and built-in function calls across all backends.

### Scope
- **In Scope**:
    - Modifying `src/codegen.c` to handle `println`/`print` in `AST_CALL`.
    - Modifying `src/codegen_arm64.c` and `src/codegen_armv6.c` for consistency.
    - Updating `tests/asm_test.rs` to use idiomatic Rust-like syntax.
- **Out of Scope**:
    - Changing the parser to force `!` for all macros (keeping existing flexibility).
    - Implementing a full standard library for printing.

# Technical Design

### Current Implementation
- The parser distinguishes between `println!(...)` (macro call) and `println(...)` (function call).
- `src/codegen.c` handles `println` macros by mapping them to `printf` with some formatting logic for `{}`.
- Regular function calls are emitted as-is, so `println(...)` becomes `println(...)` in C, which is not defined.

### Proposed Changes
#### 1. C Backend (`src/codegen.c`)
In the `AST_CALL` case, add a check for `println` and `print` and map them to `printf`.

```c
case AST_CALL: {
    char *name = node->data.call.name;
    if (strcmp(name, "println") == 0 || strcmp(name, "print") == 0) {
        fprintf(out, "printf(");
        // ... evaluate arguments ...
        fprintf(out, ")");
        if (strcmp(name, "println") == 0) fprintf(out, "; printf(\"\\n\")");
    } else {
        // existing logic
    }
}
```

#### 2. Assembly Backends (`src/codegen_arm64.c`, `src/codegen_armv6.c`)
Apply similar logic to `AST_CALL` in the assembly backends to ensure they also support these built-in calls by branching to the `printf` symbol.

#### 3. Test Update
Update `tests/asm_test.rs`:
```rust
fn main() {
    let x = 42;
    let y = 10;
    let z = x + y;
    println!("Result is: {}", z); // Changed from println("Result is: %d\n", z);
}
```

### Risks
- **Double Newlines**: If the user uses `println("...\n", ...)` and the compiler adds another `\n`, it might be unexpected. However, since `println` isn't a standard C function, any use of it is already "non-standard" and we should prefer consistency with the macro version.

# Testing

### Validation Approach
- Run the `asm_test` using the existing test runner and verify it passes.
- Verify the generated C code for `tests/asm_test.rs` to ensure it uses `printf`.
- Run other tests to ensure no regressions in printing logic.

### Key Scenarios
- `println` as a function call with multiple arguments.
- `print` as a function call.
- `println!` as a macro call (should still work as before).

# Delivery Steps

### ✓ Step 1: Implement println/print support in C codegen for function calls
Update `src/codegen.c` to handle `AST_CALL` nodes with name `println` or `print`.
- Map `println` and `print` function calls to C `printf`.
- This ensures that code using `println(...)` instead of `println!(...)` still generates valid C code.

### ✓ Step 2: Update assembly backends for println/print function calls
Update `src/codegen_arm64.c` and `src/codegen_armv6.c` to handle `println` and `print` function calls.
- Ensure that these function calls are translated to `bl printf` (or `bl _printf` on macOS) with arguments passed in the correct registers.
- This maintains consistency across all backends.

### ✓ Step 3: Update asm_test.rs to use idiomatic println! macro
Modify `tests/asm_test.rs` to use the idiomatic `println!` macro instead of the `println` function.
- Change `println("Result is: %d\\n", z);` to `println!("Result is: {}", z);`.
- This follows Rust conventions and verifies that the macro-based printing works as expected.