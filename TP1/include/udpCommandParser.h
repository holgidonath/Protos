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
    INVALID,
    STATS_OK,
    LOCALE_EN_OK,
    LOCALE_ES_OK
};

int udpParseCommand(char * buffer);
