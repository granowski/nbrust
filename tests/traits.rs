trait Summary {
    fn summarize(&self) -> &str;
}

struct Post {
    title: &str,
    author: &str,
}

impl Summary for Post {
    fn summarize(&self) -> &str {
        return self.title;
    }
}

fn main() {
    let post = Post { title: "Traits in Rust", author: "Junie" };
    // Trait objects and dynamic dispatch would go here
    println!("Summary: {}", post.summarize());
}
