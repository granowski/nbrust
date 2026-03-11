pub use core::ptr::*;

pub struct Allocator;

impl Allocator {
    pub fn alloc(&self, size: usize) -> *mut u8 {
        malloc(size)
    }
    
    pub fn dealloc(&self, ptr: *mut u8) {
        free(ptr)
    }
}
