### Bootstrapping `rustc` and `cargo` with `nbrust`

The `nbrust` and `nbcargo` toolchain is now capable of handling structured, multi-file Rust projects. This document outlines how to use it for bootstrapping larger projects.

#### Toolchain Components

1. **`nbrust`**: The core Rust-to-C transpiler.
   - Now supports `mod name;` to recursively parse external `.rs` files.
   - Automatically handles `extern crate std;` by processing `std/src/lib.rs`.
   - Generates C code with necessary `#include <stdio.h>` for `println!`.
   - Supports sum-type enums (tagged unions), traits, and vtables for dynamic dispatch.

2. **`nbcargo`**: The build orchestrator.
   - Use `nbcargo build` in a project directory containing `Cargo.toml`.
   - It will automatically identify `src/main.rs` or `src/lib.rs` and invoke `nbrust`.
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

#### Current Bootstrapping Limitations

To reach full `rustc` parity, the following are still needed in a future expansion:

- **Type Checking**: `nbrust` currently assumes code is valid Rust and does minimal type checking during C mapping.
- **Full Macro Expansion**: While `println!`, `panic!`, and basic `vec!` are handled, complex `macro_rules!` with advanced pattern matching need a full expansion engine.
- **Dependency Management**: `nbcargo` currently only handles the local package; it needs logic to fetch and build dependencies from `crates.io`.
- **Borrow Checking**: Memory safety is delegated to the C memory model. Pointers are used where Rust would use references.
