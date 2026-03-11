pub enum Result<T, E> {
    Ok(T),
    Err(E),
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
}

pub trait Iterator {
    type Item;
    fn next(&mut self) -> Option<Self::Item>;
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

impl<T> Vec<T> {
    pub fn new() -> Self {
        Vec { data: 0 as *mut T, len: 0, cap: 0 }
    }
    pub fn len(&self) -> usize {
        self.len
    }
}
