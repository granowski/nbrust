pub struct PathBuf {
    inner: String,
}

impl PathBuf {
    pub fn new() -> PathBuf {
        PathBuf { inner: String::new() }
    }

    pub fn push(&mut self, s: &str) {
        // ...
    }
}

pub struct Path {
    inner: str,
}
