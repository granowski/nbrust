fn main() {
    let mut vec: Vec<i32> = Vec::new();
    vec.push(1);
    vec.push(2);
    
    let res: Result<i32, &str> = Result::Ok(10);
    match res {
        Ok(val) => println!("Value: {}", val),
        Err(e) => println!("Error: {}", e),
    }

    if let Ok(val) = res {
        println!("If let got: {}", val);
    }
}
