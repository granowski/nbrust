fn main() {
    let x = 10;
    let y = 20;
    let z = x + y;
    println!("Result: {}", z);
    
    if z > 25 {
        println!("Greater than 25");
    } else {
        println!("Less than or equal to 25");
    }
    
    let mut i = 0;
    while i < 3 {
        println!("Loop: {}", i);
        i = i + 1;
    }
}
