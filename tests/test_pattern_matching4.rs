// test_pattern_matching.rs

// Define a simple enum for pattern matching tests
enum Color {
    Red,
    Blue,
    Green,
    Rainbow(u8),
}

fn main() {
    // Basic enum pattern matching
    let color = Color::Red;
    match color {
        Color::Red => println!("Matched Red"),
        Color::Blue => println!("Matched Blue"),
        Color::Green => println!("Matched Green"),
        Color::Rainbow(1) => println!("Matched Rainbow 1"),
        _ => println!("Default case"),
    }

    // Or-patterns: match multiple variants
    let value = 42;
    match value {
        1 | 2 | 3 => println!("Small number"),
        10..=20 => println!("Medium number"),
        _ => println!("Large number"),
    }

    // Range patterns with guards
    let num = 5;
    match num {
        1..=10 if num % 2 == 0 => println!("Even number in range"),
        1..=10 => println!("Odd number in range"),
        _ => println!("Out of range"),
    }

    // Guarded pattern matching
    let msg = Color::Rainbow(3);
    match msg {
        Color::Rainbow(n) if n > 5 => println!("Rainbow number > 5: {}", n),
        Color::Rainbow(n) => println!("Rainbow number: {}", n),
        _ => println!("Not a rainbow"),
    }
}
