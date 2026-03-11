enum Status {
    Active,
    Inactive,
    Pending,
}

fn print_status(s: Status) {
    match s {
        Status::Active => {
            println!("Status is Active");
        },
        Status::Inactive => {
            println!("Status is Inactive");
        },
        _ => {
            println!("Status is something else");
        }
    }
}

fn main() {
    let s = Status::Active;
    print_status(s);
}
