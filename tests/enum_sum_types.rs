enum Result<T, E> {
    Ok(T),
    Err(E),
}

fn main() {
    let res: Result<i32, &str> = Result::Ok(42);
    match res {
        Result::Ok(val) => println!("Success: {}", val),
        Result::Err(e) => println!("Error: {}", e),
    }
}
