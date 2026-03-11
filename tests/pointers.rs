fn main() {
    let x = 42;
    let y = &x;
    let z = *y;
    println!("x: {}, z: {}", x, z);

    let b = Box::new(100);
    let val = *b;
    println!("Box value: {}", val);
}
