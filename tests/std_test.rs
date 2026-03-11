extern crate std;
use std::Option;
use std::Vec;

fn main() {
    let o = Option::Some(42);
    let v: Vec<i32> = Vec::new();
    println!("Vector len: %d", v.len());
    
    match o {
        Option::Some(x) => println!("Value: %d", x),
        Option::None => println!("None"),
    }
}
