use io::File;

pub fn read_to_string(path: &str) -> String {
    let mut file = File::open(path);
    // ... Simplified, nbrust doesn't support reading into a string yet
    String::new()
}

pub fn write(path: &str, content: &str) {
    let mut file = File::open(path);
    // ...
}
