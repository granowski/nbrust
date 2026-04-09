use io::{self, File, Read, Write};

extern "C" {
    fn unlink(path: &str) -> i32;
    #[link_name = "rename"]
    fn rename_c(from: &str, to: &str) -> i32;
}

pub fn read(path: &str) -> io::Result<Vec<u8>> {
    let mut file = File::open(path)?;
    let mut buf = Vec::new();
    file.read_to_end(&mut buf)?;
    Ok(buf)
}

pub fn read_to_string(path: &str) -> io::Result<String> {
    let buf = read(path)?;
    // In a real Rust, we'd validate UTF-8 here
    Ok(String { vec: buf })
}

pub fn write(path: &str, contents: &[u8]) -> io::Result<()> {
    let mut file = File::create(path)?;
    file.write_all(contents)?;
    Ok(())
}

pub fn remove_file(path: &str) -> io::Result<()> {
    let res = unsafe { unlink(path) };
    if res < 0 {
        Err(io::Error::Generic(res))
    } else {
        Ok(())
    }
}

pub fn rename(from: &str, to: &str) -> io::Result<()> {
    let res = unsafe { rename_c(from, to) };
    if res < 0 {
        Err(io::Error::Generic(res))
    } else {
        Ok(())
    }
}
