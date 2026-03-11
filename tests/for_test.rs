extern crate std;

fn main() {
    let mut v = Vec::new();
    v.push(1);
    v.push(2);
    for x in v {
        println!("Value: {}", x);
    }
}
