macro_rules! my_vec {
    ( $( $x:expr ),* ) => {
        {
            let mut v = Vec::new();
            $( v.push($x); )*
            v
        }
    };
}

fn main() {
    let v = my_vec![1, 2, 3];
    println!("Vector element: %d", v[0]);
}
