### nbrust Project Task List

This roadmap details the remaining features required to complete the `nbrust` compiler implementation, based on "The Rust Programming Language.pdf" and the goal of achieving robust, safe, and efficient C23 transpilation.

#### 1. Borrow Checker & Lifetimes
- [ ] **Non-Lexical Lifetimes (NLL)**: Move from scope-based borrow checking to a control-flow graph (CFG)-based analysis for more flexible borrowing.
- [ ] **Lifetime Annotations**: Support parsing and validating explicit lifetime syntax (`'a`, `'static`).
- [ ] **Ref Cell & Interior Mutability**: Implement `RefCell` and `Cell` wrappers in `std`.

#### 2. Advanced Pattern Matching
- [ ] **Nested Patterns**: Full support for deep patterns (e.g., `Some(Ok(x))`).
- [ ] **Match Guards**: Support `if` conditions in match arms.
- [ ] **Ranges and OR Patterns**: Implement `1..=5` and `A | B` in patterns.
- [ ] **Exhaustiveness Checking**: Implement a check to ensure all possible enum variants or values are covered.

#### 3. Ownership & Memory Management
- [ ] **`Drop` Trait**: Implement automatic generation of destructor calls at the end of variable lifetimes.
- [ ] **Reference Counting**: Implement `std::rc::Rc` and `std::sync::Arc` (the latter requiring atomics).
- [ ] **Pinning**: Support the `Pin` wrapper for self-referential structures.

#### 4. Concurrency Primitives
- [ ] **Thread Spawning**: Add `std::thread::spawn` with a C backend mapping to `pthread` or C11 threads.
- [ ] **Mutual Exclusion**: Implement `std::sync::Mutex` and `std::sync::RwLock` wrapping C primitives.
- [ ] **Channels**: Implement `std::sync::mpsc` for message passing.
- [ ] **Send & Sync Traits**: Enforce thread-safety in the type checker using auto-traits.

#### 5. Standard Library Expansion
- [ ] **Robust `String`**: Implement a proper UTF-8 aware `String` and `&str`.
- [ ] **I/O & File System**: Expand `std::io` and implement `std::fs` (Open, Read, Write).
- [ ] **Networking**: Add `std::net` for TCP/UDP sockets.
- [ ] **Collections**: Complete `HashMap` (with a real hash trait like `SipHash`), `VecDeque`, and `BTreeMap`.

#### 6. Compiler & Tooling
- [ ] **Procedural Macros**: Add support for `derive` and attribute-like macros.
- [ ] **Error Recovery**: Improve the parser to recover from syntax errors and report multiple diagnostics.
- [ ] **LLVM Backend (Optional)**: Explore an alternative backend for better optimization.
