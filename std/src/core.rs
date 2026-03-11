pub mod ptr {
    extern "C" {
        pub fn malloc(size: usize) -> *mut u8;
        pub fn free(ptr: *mut u8);
        pub fn memcpy(dest: *mut u8, src: *const u8, n: usize) -> *mut u8;
        pub fn realloc(ptr: *mut u8, size: usize) -> *mut u8;
    }
}
