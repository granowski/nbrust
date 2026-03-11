struct Wrapper<T> {
    value: T,
}

fn wrap_i32(val: i32) -> Wrapper<i32> {
    return Wrapper { value: val };
}

fn main() {
    let w = wrap_i32(42);
    println!("Wrapped: {}", w.value);
}
