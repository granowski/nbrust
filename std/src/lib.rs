pub use io::File;
pub use fs::*;
pub use collections::*;
pub use path::*;
pub use alloc::*;
pub use core::*;

pub mod io;
pub mod collections;
pub mod fs;
pub mod path;
pub mod alloc;
pub mod core;

pub fn print_hello() {
    println!("Hello from std!");
}

pub enum Result<T, E> {
    Ok(T),
    Err(E),
}

pub enum Option<T> {
    Some(T),
    None,
}

pub struct Vec<T> {
    data: *mut T,
    len: usize,
    cap: usize,
}
