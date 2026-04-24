// test_pattern_matching.rs
fn main() {
    // Or-patterns: match multiple variants
    let value = 42;
    match value {
        1 | 2 | 3 => println!("Small number"),
        10..=20 => println!("Medium number"),
        _ => println!("Large number"),
    }
}
