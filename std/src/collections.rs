pub struct Vec<T> {
    ptr: *mut T,
    len: usize,
    cap: usize,
}

impl<T> Vec<T> {
    pub fn new() -> Vec<T> {
        Vec {
            ptr: 0 as *mut T,
            len: 0,
            cap: 0,
        }
    }

    pub fn with_capacity(capacity: usize) -> Vec<T> {
        let size = capacity * 8; // simplified
        let p = malloc(size) as *mut T;
        Vec {
            ptr: p,
            len: 0,
            cap: capacity,
        }
    }

    pub fn push(&mut self, value: T) {
        if self.len == self.cap {
            let new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
            let new_ptr = realloc(self.ptr as *mut u8, new_cap * 8) as *mut T;
            self.ptr = new_ptr;
            self.cap = new_cap;
        }
        // * (self.ptr + self.len) = value; // nbrust doesn't support this yet
        // self.len += 1;
    }
}

pub struct String {
    vec: Vec<u8>,
}

impl String {
    pub fn new() -> String {
        String {
            vec: Vec::new(),
        }
    }

    pub fn from(s: &str) -> String {
        // ...
        String { vec: Vec::new() }
    }
}
