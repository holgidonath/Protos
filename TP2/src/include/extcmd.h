#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include <stdbool.h>

#include "buffer.h"
#include "selector.h"


#define READ 0
#define WRITE 1
struct copy
{
    int *fd;
    buffer *rb, *wb;
    fd_interest duplex;
    struct copy *other;

};

struct extern_cmd{
    buffer                  *mail_buffer;
    buffer                  *filtered_mail_buffer;
    buffer                  *wb;
    struct parser           *multi_parser;
    struct copy copy_filter;
    int     pipe_in[2];
    int     pipe_out[2];
};

void
env_var_init(void);


#endif
