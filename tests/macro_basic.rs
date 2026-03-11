macro_rules! say_hello {
    () => {
        println!("Hello from macro!");
    };
    ($name:expr) => {
        println!("Hello, {}!", $name);
    };
}

fn main() {
    say_hello!();
    say_hello!("World");
}
