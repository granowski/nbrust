// test_pattern_matching.rs

// Define a simple enum for pattern matching tests
enum Color {
    Red,
    Blue,
    Green,
}

fn main() {
    // Basic enum pattern matching
    let color = Color::Red;
    match color {
        Color::Red => println!("Matched Red"),
        Color::Blue => println!("Matched Blue"),
        Color::Green => println!("Matched Green"),
        _ => println!("Default case"),
    }
}
