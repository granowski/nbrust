### Bootstrapping `rustc` and `cargo` with `nbrust`

The `nbrust` and `nbcargo` toolchain is now capable of handling structured, multi-file Rust projects. This document outlines how to use it for bootstrapping larger projects like `rustc` and `cargo` themselves.

#### Toolchain Components

1. **`nbrust`**: The core Rust-to-C transpiler.
   - Supports `mod name;` to recursively parse external `.rs` files.
   - Automatically handles `extern crate std;` by processing `std/src/lib.rs`.
   - Generates C23 code with necessary `#include` directives for `println!`, `malloc`, etc.
   - Supports sum-type enums (tagged unions), traits, and vtables for dynamic dispatch.
   - Includes early-stage passes for **Macro Expansion**, **Monomorphization**, **Type Checking**, and **Borrow Checking**.

2. **`nbcargo`**: The build orchestrator.
   - Use `nbcargo build` in a project directory containing `Cargo.toml`.
   - Automatically identifies `src/main.rs` or `src/lib.rs` and invokes `nbrust`.
   - Orchestrates dependency resolution, including fetching crates from crates.io into a local `vendor/` directory.
   - After transpilation, it calls `cc` to compile the resulting `.c` file into a binary named after the project in `Cargo.toml`.
   - Use `nbcargo run` to build and immediately execute the project.

#### Steps for Bootstrapping

1. **Setup Project Structure**:
   Ensure your project follows the standard Cargo structure:
   ```
   my_project/
   ├── Cargo.toml
   └── src/
       ├── main.rs
       └── sub.rs
   ```

2. **Build and Run**:
   From the project root:
   ```bash
   ./nbcargo run Cargo.toml
   ```

3. **Self-Hosting Journey**:
   To use `nbrust` to compile `rustc`:
   - Point `nbrust` to the `rustc` source entry point (`src/librustc_driver/lib.rs` or similar).
   - Ensure `extern crate std;` is handled. `nbrust` will recursively resolve the thousands of modules in `rustc`.
   - Currently, `rustc` uses thousands of files, so scalability of the parser and memory management in `nbrust` is a key focus.

#### Current Bootstrapping Gaps

To reach full `rustc` parity, the following are still needed in the `nbrust` toolchain:

- **Borrow Checker**: The current implementation is a skeleton. Full lexical/non-lexical lifetimes (NLL) and proper data-flow analysis are required for `rustc`'s complex borrow patterns.
- **Full Macro Expansion**: While basic `macro_rules!` and built-in macros like `vec!` and `matches!` are supported, a full token-based expansion engine with hygiene is needed for `rustc`'s heavy macro usage.
- **Standard Library**: `std/src/lib.rs` is currently a collection of stubs. It needs significant expansion (I/O, process management, advanced collections) to support a self-hosting compiler.
- **Ownership & RAII**: Proper support for the `Drop` trait and deterministic destruction must be implemented in the C backend to manage memory correctly without a garbage collector.
- **Diagnostics**: Improved error reporting with spans and suggestions to help debug the bootstrap process.
