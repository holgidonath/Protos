#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include <stdbool.h>

#include "buffer.h"
#include "selector.h"

#define READ 0
#define WRITE 1

struct extern_cmd {
    buffer                  *mail_buffer;
    buffer                  *filtered_mail_buffer;
    buffer                  *wb;
    struct parser           *multi_parser;
};

void
env_var_init(char *username);


#endif
