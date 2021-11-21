#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include "buffer.h"
#include <stdbool.h>
#include "selector.h"

#define READ 0
#define WRITE 1

void env_var_init(char *username);


struct extern_cmd {
    buffer                  *mail_buffer;
    buffer                  *filtered_mail_buffer;
    buffer                  *wb;
    struct parser           *multi_parser;
};

#endif //PROTOS_EXTCMD_H
