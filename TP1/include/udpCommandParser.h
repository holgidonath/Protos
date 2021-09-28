#include <stdint.h>

enum cmdState{
    BEGIN,
    L,
    LO,
    LOC,
    LOCA,
    LOCAL,
    LOCALE,
    E,
    ES,
    EN,
    S,
    ST,
    STA,
    STAT,
    STATS,
    SE,
    SET,
    RETURN,
    FINISH,
    INVALID
};

int parseCommand(char * buffer);
