pub struct Box<T> {
    ptr: *mut T,
}

impl<T> Box<T> {
    pub fn new(x: T) -> Self {
        let ptr: *mut T = unsafe { malloc(sizeof(T)) as *mut T };
        unsafe { *ptr = x };
        Box { ptr: ptr }
    }
}

impl<T> Box<T> {
    pub fn as_ptr(&self) -> *const T {
        self.ptr
    }
    pub fn as_mut_ptr(&mut self) -> *mut T {
        self.ptr
    }
}

impl<T> Box<T> {
    pub fn leak(self) -> *mut T {
        self.ptr
    }
}

pub fn panic(msg: &str) -> ! {
    println!("panic: {}", msg);
    unsafe { exit(1) };
}

pub enum Result<T, E> {
    Ok(T),
    Err(E),
}

impl<T, E> Result<T, E> {
    pub fn is_ok(&self) -> bool {
        match self {
            Result::Ok(_) => true,
            Result::Err(_) => false,
        }
    }
    pub fn is_err(&self) -> bool {
        !self.is_ok()
    }
    pub fn unwrap(self) -> T {
        match self {
            Result::Ok(val) => val,
            Result::Err(_) => panic("called `Result::unwrap()` on an `Err` value"),
        }
    }
}

impl<T, E> Result<T, E> {
    pub fn expect(self, msg: &str) -> T {
        match self {
            Result::Ok(val) => val,
            Result::Err(_) => panic(msg),
        }
    }
}

pub struct HashMap<K, V> {
    // Stub
    size: usize,
}

impl<K, V> HashMap<K, V> {
    pub fn new() -> Self {
        HashMap { size: 0 }
    }
    pub fn insert(&mut self, k: K, v: V) -> Option<V> {
        Option::None
    }
    pub fn get(&self, k: K) -> Option<&V> {
        Option::None
    }
}

pub struct VecDeque<T> {
    // Stub
    vec: Vec<T>,
}

impl<T> VecDeque<T> {
    pub fn new() -> Self {
        VecDeque { vec: Vec::new() }
    }
    pub fn push_back(&mut self, item: T) {
        self.vec.push(item);
    }
    pub fn pop_front(&mut self) -> Option<T> {
        Option::None
    }
}

pub struct String {
    vec: Vec<u8>,
}

impl String {
    pub fn new() -> Self {
        String { vec: Vec::new() }
    }
    pub fn len(&self) -> usize {
        self.vec.len()
    }
    pub fn push(&mut self, ch: u8) {
        self.vec.push(ch);
    }
}

pub trait Iterator {
    type Item;
    fn next(&mut self) -> Option<Self::Item>;
}

pub trait IntoIterator {
    type Item;
    type IntoIter: Iterator<Item = Self::Item>;
    fn into_iter(self) -> Self::IntoIter;
}

pub enum Option<T> {
    Some(T),
    None,
}

impl<T> Option<T> {
    pub fn is_some(&self) -> bool {
        match self {
            Option::Some(_) => true,
            Option::None => false,
        }
    }
    pub fn is_none(&self) -> bool {
        !self.is_some()
    }
    pub fn unwrap(self) -> T {
        match self {
            Option::Some(val) => val,
            Option::None => panic("called `Option::unwrap()` on a `None` value"),
        }
    }
}

impl<T> Option<T> {
    pub fn expect(self, msg: &str) -> T {
        match self {
            Option::Some(val) => val,
            Option::None => panic(msg),
        }
    }
}

pub struct Vec<T> {
    data: *mut T,
    len: usize,
    cap: usize,
}

impl<T> Vec<T> {
    pub fn new() -> Self {
        Vec { data: 0 as *mut T, len: 0, cap: 0 }
    }
    pub fn len(&self) -> usize {
        self.len
    }
}

impl<T> Vec<T> {
    pub fn with_capacity(capacity: usize) -> Self {
        let data: *mut T = unsafe { malloc(capacity * sizeof(T)) as *mut T };
        Vec { data: data, len: 0, cap: capacity }
    }
}
impl<T> Vec<T> {
    pub fn push(&mut self, item: T) {
        if self.len == self.cap {
            let new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
            let new_data: *mut T = unsafe { realloc(self.data as *mut void, new_cap * sizeof(T)) as *mut T };
            self.data = new_data;
            self.cap = new_cap;
        }
        unsafe { *(self.data + self.len) = item };
        self.len = self.len + 1;
    }
}
impl<T> Vec<T> {
    pub fn pop(&mut self) -> Option<T> {
        if self.len == 0 {
            Option::None
        } else {
            self.len = self.len - 1;
            Option::Some(unsafe { *(self.data + self.len) })
        }
    }
}
impl<T> Vec<T> {
    pub fn is_empty(&self) -> bool {
        self.len == 0
    }
    pub fn get(&self, index: usize) -> Option<&T> {
        if index < self.len {
            Option::Some(unsafe { (self.data + index) as &T })
        } else {
            Option::None
        }
    }
}
}

pub struct VecIter<T> {
    vec_ptr: *const T,
    len: usize,
    index: usize,
}

impl<T> Iterator for VecIter<T> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        if self.index < self.len {
            let val = unsafe { *(self.vec_ptr + self.index) };
            self.index = self.index + 1;
            Option::Some(val)
        } else {
            Option::None
        }
    }
}

impl<T> IntoIterator for Vec<T> {
    type Item = T;
    type IntoIter = VecIter<T>;
    fn into_iter(self) -> VecIter<T> {
        VecIter {
            vec_ptr: self.data,
            len: self.len,
            index: 0,
        }
    }
}

pub struct HashMap<K, V> {
    buckets: Vec<Option<HashEntry<K, V>>>,
    size: usize,
}

struct HashEntry<K, V> {
    key: K,
    value: V,
}

impl<K, V> HashMap<K, V> {
    pub fn new() -> Self {
        let mut buckets = Vec::new();
        // Initialize with some buckets
        let mut i = 0;
        while i < 16 {
            buckets.push(Option::None);
            i = i + 1;
        }
        HashMap { buckets: buckets, size: 0 }
    }
    pub fn insert(&mut self, k: K, v: V) {
        // Very simple "hash": just use some property if we could, 
        // but for now, let's just push or find an empty slot.
        // This is a stub that actually stores data now.
        self.buckets.push(Option::Some(HashEntry { key: k, value: v }));
        self.size = self.size + 1;
    }
    pub fn get(&self, k: K) -> Option<&V> {
        // Simple linear search for now since we don't have a real hash() trait yet
        let mut i = 0;
        while i < self.buckets.len() {
            match self.buckets.get(i) {
                Option::Some(entry_opt) => {
                    match entry_opt {
                        Option::Some(entry) => {
                            // if entry.key == k { return Option::Some(&entry.value); }
                            // For now, return None or first one to avoid == issues
                        }
                        Option::None => {}
                    }
                }
                Option::None => {}
            }
            i = i + 1;
        }
        Option::None
    }
}
