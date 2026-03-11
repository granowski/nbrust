struct Point {
    x: i32,
    y: i32,
}

impl Point {
    fn new(x: i32, y: i32) -> Self {
        Point { x: x, y: y }
    }

    fn get_x(&self) -> i32 {
        self.x
    }

    fn origin() -> Self {
        Self { x: 0, y: 0 }
    }
}

fn main() {
    let p = Point::new(10, 20);
    println!("p.x = %d", p.get_x());
    let o = Point::origin();
    println!("o.x = %d", o.get_x());
}
