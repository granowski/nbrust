enum Outer {
    Inner(i32, i32),
    Other,
}

fn main() {
    let x = Outer::Inner(10, 20);
    match x {
        Outer::Inner(10, y) => {
            println!("Nested match: y = {}", y);
        }
        Outer::Inner(x, y) => {
            println!("Fallback match: x = {}, y = {}", x, y);
        }
        _ => {
            println!("Other");
        }
    }
}
