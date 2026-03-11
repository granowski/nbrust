fn main() {
    let x = String::from("hello");
    {
        let r = &x;
        println!("{}", r);
    } // r's borrow should be released here
    let y = x; // This should be valid after r goes out of scope
    println!("{}", y);
}
