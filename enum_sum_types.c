// Program started
// Source read, len=240
// Source start: enum Result<T, E> {

// Initializing lexer
// Initializing parser object
// Initializing parser
// current token: enum (type=37)
// next token: Result (type=3)
// Parser initialized
enum Result_tag { Result_Ok, Result_Err };
struct Result {
    enum Result_tag tag;
    union {
        struct T Ok;
        struct E Err;
    } data;
};
