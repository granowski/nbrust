pub enum Error {
    Generic(i32),
    NotFound,
    PermissionDenied,
    ConnectionRefused,
    ConnectionReset,
    ConnectionAborted,
    NotConnected,
    AddrInUse,
    AddrNotAvailable,
    BrokenPipe,
    AlreadyExists,
    WouldBlock,
    InvalidInput,
    InvalidData,
    TimedOut,
    WriteZero,
    Interrupted,
    Other,
    UnexpectedEof,
}

pub type Result<T> = crate::Result<T, Error>;

pub trait Read {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize>;
    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> Result<usize> {
        let mut count = 0;
        let mut chunk = [0u8; 1024];
        loop {
            match self.read(&mut chunk) {
                Ok(0) => break,
                Ok(n) => {
                    let mut i = 0;
                    while i < n {
                        buf.push(chunk[i]);
                        i = i + 1;
                    }
                    count = count + n;
                }
                Err(e) => return Err(e),
            }
        }
        Ok(count)
    }
}

pub trait Write {
    fn write(&mut self, buf: &[u8]) -> Result<usize>;
    fn flush(&mut self) -> Result<()>;
    fn write_all(&mut self, mut buf: &[u8]) -> Result<()> {
        while !buf.is_empty() {
            match self.write(buf) {
                Ok(0) => return Err(Error::WriteZero),
                Ok(n) => buf = &buf[n..],
                Err(e) => return Err(e),
            }
        }
        Ok(())
    }
}

pub struct File {
    fd: i32,
}

impl File {
    pub fn open(path: &str) -> Result<File> {
        let fd = unsafe { open(path, 0) }; // O_RDONLY is usually 0
        if fd < 0 {
            Err(Error::Generic(fd))
        } else {
            Ok(File { fd: fd })
        }
    }

    pub fn create(path: &str) -> Result<File> {
        // O_WRONLY | O_CREAT | O_TRUNC is usually 0x0001 | 0x0200 | 0x0400 on many systems,
        // but we'll need better flag handling eventually.
        let fd = unsafe { open(path, 0x0001 | 0x0200 | 0x0400) };
        if fd < 0 {
            Err(Error::Generic(fd))
        } else {
            Ok(File { fd: fd })
        }
    }
}

impl Read for File {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        let n = unsafe { read(self.fd, buf.as_mut_ptr(), buf.len()) };
        if n < 0 {
            Err(Error::Generic(n as i32))
        } else {
            Ok(n)
        }
    }
}

impl Write for File {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        let n = unsafe { write(self.fd, buf.as_ptr(), buf.len()) };
        if n < 0 {
            Err(Error::Generic(n as i32))
        } else {
            Ok(n)
        }
    }

    fn flush(&mut self) -> Result<()> {
        Ok(()) // fsync could go here if needed
    }
}

impl File {
    pub fn close(self) -> Result<()> {
        let res = unsafe { close(self.fd) };
        if res < 0 {
            Err(Error::Generic(res))
        } else {
            Ok(())
        }
    }
}

pub struct Stdin;
pub struct Stdout;
pub struct Stderr;

impl Read for Stdin {
    fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
        let n = unsafe { read(0, buf.as_mut_ptr(), buf.len()) };
        if n < 0 {
            Err(Error::Generic(n as i32))
        } else {
            Ok(n)
        }
    }
}

impl Write for Stdout {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        let n = unsafe { write(1, buf.as_ptr(), buf.len()) };
        if n < 0 {
            Err(Error::Generic(n as i32))
        } else {
            Ok(n)
        }
    }

    fn flush(&mut self) -> Result<()> {
        Ok(())
    }
}

impl Write for Stderr {
    fn write(&mut self, buf: &[u8]) -> Result<usize> {
        let n = unsafe { write(2, buf.as_ptr(), buf.len()) };
        if n < 0 {
            Err(Error::Generic(n as i32))
        } else {
            Ok(n)
        }
    }

    fn flush(&mut self) -> Result<()> {
        Ok(())
    }
}

pub fn stdout() -> Stdout {
    Stdout
}

pub fn stderr() -> Stderr {
    Stderr
}

pub fn stdin() -> Stdin {
    Stdin
}

extern "C" {
    pub fn open(path: &str, flags: i32) -> i32;
    pub fn read(fd: i32, buf: *mut u8, count: usize) -> isize;
    pub fn write(fd: i32, buf: *const u8, count: usize) -> isize;
    pub fn close(fd: i32) -> i32;
    pub fn lseek(fd: i32, offset: isize, whence: i32) -> isize;
}
