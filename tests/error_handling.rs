enum Option<T> {
    Some(T),
    None,
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

fn divide(a: i32, b: i32) -> Result<i32, &str> {
    if b == 0 {
        return Result::Err("Cannot divide by zero");
    } else {
        return Result::Ok(a / b);
    }
}

fn main() {
    let res = divide(10, 2);
    match res {
        Result::Ok => {
            println!("Result is Ok");
        },
        Result::Err => {
            println!("Error occurred");
        },
    }
}
