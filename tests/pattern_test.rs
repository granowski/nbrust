fn main() {
    let res: Result<i32, &str> = Result::Ok(10);
    match res {
        Ok(val) => println!("Value: {}", val),
        Err(e) => println!("Error: {}", e),
    }
}
