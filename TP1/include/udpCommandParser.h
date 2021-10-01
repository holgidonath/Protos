#include <stdint.h>

enum udpCmdState{
    UDP_BEGIN,
    L,
    LO,
    LOC,
    LOCA,
    LOCAL,
    LOCALE,
    LOCALESPACE,
    UDP_E,
    ES,
    EN,
    S,
    ST,
    STA,
    STAT,
    STATS,
    SE,
    SET,
    SETSPACE,
    RETURN,
    UDP_FINISH,
    UDP_INVALID,
    STATS_OK,
    LOCALE_EN_OK,
    LOCALE_ES_OK
};

int udpParseCommand(char * buffer);
