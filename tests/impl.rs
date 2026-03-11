struct Rectangle {
    width: i32,
    height: i32,
}

impl Rectangle {
    fn area(&self) -> i32 {
        return self.width * self.height;
    }
}

fn main() {
    let rect: Rectangle;
    rect.width = 10;
    rect.height = 20;
    let a = rect.area();
    return a;
}
