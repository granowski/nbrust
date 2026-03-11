enum Message {
    Quit,
    Move(i32),
    Write(&str),
}

fn process_message(msg: Message) {
    match msg {
        Message::Quit => {
            println!("Quit");
        },
        Message::Move => {
            println!("Move");
        },
        Message::Write => {
            println!("Write");
        },
    }
}

fn main() {
    let msg = Message::Quit;
    process_message(msg);
}
