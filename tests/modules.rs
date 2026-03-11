mod math {
    fn add(a: i32, b: i32) -> i32 {
        return a + b;
    }
}

use math::add;

fn main() {
    let result = add(5, 10);
    println!("Result: {}", result);
}
