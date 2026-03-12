enum Message {
    Quit,
    Move { x: i32, y: i32 },
    Write(&str),
    ChangeColor(i32, i32, i32),
}

fn main() {
    let msg1 = Message::Quit;
    match msg1 {
        Message::Quit => println!("Quit"),
        _ => println!("Other"),
    }

    let msg2 = Message::Move { x: 10, y: 20 };
    match msg2 {
        Message::Move { x, y } => println!("Move to %d, %d", x, y),
        _ => println!("Other"),
    }

    let msg3 = Message::Write("hello");
    match msg3 {
        Message::Write(text) => println!("Write: %s", text),
        _ => println!("Other"),
    }

    let msg4 = Message::ChangeColor(255, 0, 0);
    match msg4 {
        Message::ChangeColor(r, g, b) => println!("Color: %d, %d, %d", r, g, b),
        _ => println!("Other"),
    }
}
