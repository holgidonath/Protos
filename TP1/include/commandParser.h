#include <stdint.h>
#include "buffer.h"

enum cmdType{
    ECHO_CMD,
    GET_CMD
};

enum cmdState{
    BEGIN,
    E,
    EC,
    ECH,
    ECHO,
    G,
    GE,
    GET,
    FINISH,
    INVALID
};

int parseCommand(char * buffer, int * commandParsed, long * valread, int * wasValid, int * limit, struct buffer * buf, char *locale, int * correct_lines, int * incorrect_lines);
