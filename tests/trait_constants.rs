trait Constants {
    const ID: i32;
    fn get_id(&self) -> i32;
}

struct MyStruct {
    value: i32,
}

impl Constants for MyStruct {
    const ID: i32 = 42;
    fn get_id(&self) -> i32 {
        self.value
    }
}

fn main() {
    let s = MyStruct { value: 100 };
    println!("ID = %d", MyStruct_Constants_ID);
    println!("Value = %d", s.get_id());
}
