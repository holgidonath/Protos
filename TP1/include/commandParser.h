#include <stdint.h>

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

int parseCommand(char * buffer, int * commandParsed, int * valread, int * wasValid, int * limit);
