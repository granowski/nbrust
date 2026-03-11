extern "C" {
    fn puts(s: *const i8) -> i32;
}

pub fn main() {
    let message = "Hello from extern C!";
    unsafe {
        puts(message);
    }
}
