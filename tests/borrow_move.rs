fn main() {
    let x = 1;
    let y = x;
    println!("x: {}", x); // Should fail borrow check
}
