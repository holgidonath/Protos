
enum getState{
    BEGIN_GET,
    D,
    DA,
    DAT,
    DATE,
    T,
    TI,
    TIM,
    TIME,
    CR,
    LF,
    FINISH_GET,
    INVALID_GET,
    INVALID_CRLF
};

int getParser(char * buffer, long int * valread, int * wasValid, int * prev_limit, int * commandParsed, struct buffer * buf, char *locale);
