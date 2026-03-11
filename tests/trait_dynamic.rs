trait Animal {
    fn speak(&self);
}

struct Dog {
    name: &str,
}

impl Animal for Dog {
    fn speak(&self) {
        println!("Woof! My name is {}", self.name);
    }
}

fn main() {
    let d = Dog { name: "Fido" };
    let animal_obj: dyn Animal = d;
    animal_obj.speak();
}
