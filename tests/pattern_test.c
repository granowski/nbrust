// parse_statement: current is let (1)
// parse_statement: current is match (39)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    Result_int___char res = Result_Ok(10);
    {
    // match
    void* _match_tmp = &(res);
    int _tag = ((struct { int tag; }*)_match_tmp)->tag;
    switch (_tag) {
        case Result_Ok: {
            #define val (((struct { int tag; union { void* Ok; } data; }*)_match_tmp)->data.Ok)
printf(Value: {}, val); printf("\n");
            #undef val
            break;
        }
        case Result_Err: {
            #define e (((struct { int tag; union { void* Err; } data; }*)_match_tmp)->data.Err)
printf(Error: {}, e); printf("\n");
            #undef e
            break;
        }
    }
    }
}
