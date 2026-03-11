struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let p = Point { x: 10, y: 20 };
    match p {
        Point { x, y } => {
            println!("x: {}, y: {}", x, y);
        }
    }
}
