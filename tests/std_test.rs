extern crate std;

fn main() {
    let mut v = Vec::new();
    v.push(1);
    v.push(2);
    v.push(3);

    println!("Vector length: {}", v.len());

    for x in v {
        println!("Value: {}", x);
    }

    let mut opt = Option::Some(42);
    if opt.is_some() {
        println!("Option is some: {}", opt.unwrap());
    }

    let res: Result<i32, &str> = Result::Ok(100);
    if res.is_ok() {
        println!("Result is ok: {}", res.unwrap());
    }
}
