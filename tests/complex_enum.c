#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum Message_tag { Message_NONE, Message_Quit, Message_Move, Message_Write, Message_ChangeColor };
struct Message {
    enum Message_tag tag;
    union {
        struct { int x; int y; } Move;
        struct { struct String _0; } Write;
        struct { int _0; int _1; int _2; } ChangeColor;
    } data;
};
static struct Message Message_Quit() {
    struct Message res; res.tag = Message_Quit;
    return res;
}
static struct Message Message_Move(int x, int y) {
    struct Message res; res.tag = Message_Move;
    res.data.Move.x = x;
    res.data.Move.y = y;
    return res;
}
static struct Message Message_Write(struct String _0) {
    struct Message res; res.tag = Message_Write;
    res.data.Write._0 = _0;
    return res;
}
static struct Message Message_ChangeColor(int _0, int _1, int _2) {
    struct Message res; res.tag = Message_ChangeColor;
    res.data.ChangeColor._0 = _0;
    res.data.ChangeColor._1 = _1;
    res.data.ChangeColor._2 = _2;
    return res;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    struct Message msg = Message_Write(/* Unknown receiver call */ hello.to_string());
    {
    // match
    void* _match_tmp = &(msg);
    int _tag = ((struct { int tag; }*)_match_tmp)->tag;
    switch (_tag) {
        case Message_Quit: {
printf(Quit); printf("\n");
            break;
        }
        case Message_Move: {
            #define x (((struct { int tag; union { struct { void* x; } Move; } data; }*)_match_tmp)->data.Move.x)
            #define y (((struct { int tag; union { struct { void* y; } Move; } data; }*)_match_tmp)->data.Move.y)
printf(Move to {}, {}, x, y); printf("\n");
            #undef x
            #undef y
            break;
        }
        case Message_Write: {
            #define text (((struct { int tag; union { struct { void* _0; } Write; } data; }*)_match_tmp)->data.Write._0)
printf(Write: {}, text); printf("\n");
            #undef text
            break;
        }
        case Message_ChangeColor: {
            #define r (((struct { int tag; union { struct { void* _0; } ChangeColor; } data; }*)_match_tmp)->data.ChangeColor._0)
            #define g (((struct { int tag; union { struct { void* _1; } ChangeColor; } data; }*)_match_tmp)->data.ChangeColor._1)
            #define b (((struct { int tag; union { struct { void* _2; } ChangeColor; } data; }*)_match_tmp)->data.ChangeColor._2)
printf(Color: {}, {}, {}, r, g, b); printf("\n");
            #undef r
            #undef g
            #undef b
            break;
        }
    }
    }
}
