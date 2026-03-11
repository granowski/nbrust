fn main() {
    let x = 10;
    if x > 5 {
        let z = 1;
        println!("x is greater than 5, z is {}", z);
    } else {
        println!("x is not greater than 5");
    }

    {
        let a = 2;
        println!("a is {}", a);
    }

    // Expression oriented check
    let y = if x > 5 { 100 } else { 200 };
    println!("y is {}", y);

    if x < 5 { 
        println!("this should not happen");
    } else { 
        println!("x is indeed >= 5");
    }
}
