fn main() {
    let x: i32 = 5;
    let y: i32 = 10;
    let z: i32 = x + y;
    println!("z = {}", z);
    
    let mut a: i32 = 1;
    let b: &mut i32 = &mut a;
    let c: &mut i32 = &mut a; // Should trigger borrow check error
}
