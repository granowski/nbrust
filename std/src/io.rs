pub struct File {
    fd: i32,
}

impl File {
    pub fn open(path: &str) -> File {
        let fd = open(path, 0); // Simplified
        File { fd: fd }
    }

    pub fn read(&self, buf: &mut [u8]) -> usize {
        read(self.fd, buf.as_mut_ptr(), buf.len())
    }

    pub fn write(&self, buf: &[u8]) -> usize {
        write(self.fd, buf.as_ptr(), buf.len())
    }
}

pub struct Stdin;
pub struct Stdout;
pub struct Stderr;

impl Stdout {
    pub fn write(&self, buf: &[u8]) -> usize {
        write(1, buf.as_ptr(), buf.len())
    }
}

pub fn stdout() -> Stdout {
    Stdout
}

extern "C" {
    pub fn open(path: &str, flags: i32) -> i32;
    pub fn read(fd: i32, buf: *mut u8, count: usize) -> usize;
    pub fn write(fd: i32, buf: *const u8, count: usize) -> usize;
    pub fn close(fd: i32) -> i32;
}
