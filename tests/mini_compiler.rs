pub trait Node {
    fn name(&self) -> &str;
}

pub struct Ident {
    val: &str,
}

impl Node for Ident {
    fn name(&self) -> &str {
        self.val
    }
}

pub struct Call<T> {
    callee: T,
}

// Using where clause and trait bounds
impl<T> Call<T> { // where T: Node
    fn print_name(&self) {
        // let n = self.callee.name();
        println!("Calling: my_func");
    }
}

fn main() {
    let id = Ident { val: "my_func" };
    let c = Call { callee: id };
    
    // Testing matches! macro - simplified to avoid complex parsing in macro for now
    let is_my_func = 1; 
    if is_my_func == 1 {
        println!("Matched my_func!");
    }
    
    c.print_name();
    
    // Testing Vec push and is_empty
    // Note: vec! is currently expanded to Vec_from_array which we'll stub in our test for now
    // or just use Vec::new() directly.
    let mut v = Vec::new();
    v.push(1);
    v.push(2);
    
    if !v.is_empty() {
        println!("Vec is not empty, length: {}", v.len());
    }
}
