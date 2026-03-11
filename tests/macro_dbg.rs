macro_rules! dbg {
    ($val:expr) => {
        println!("Value = %d", $val);
    };
}

fn main() {
    let x = 42;
    dbg!(x);
}
