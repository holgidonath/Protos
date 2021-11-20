#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include "buffer.h"
#include <stdbool.h>
#include "selector.h"

#define READ 0
#define WRITE 1

void env_var_init(char *username);

enum extern_cmd_status {
    CMD_STATUS_OK,
    CMD_STATUS_ERROR,
    CMD_STATUS_DONE,
};

struct extern_cmd {
    enum extern_cmd_status    status;

    size_t            send_bytes_write;
    size_t            send_bytes_read;

    buffer *          read_buffer;
    buffer *          write_buffer;
    buffer *          cmd_rb;
    buffer *          cmd_wb;

    int *             client_fd;
    int *             origin_fd;
    int *             read_fd;
    int *             write_fd;

    bool              done_write;
    bool              done_read;
    bool              error_write;
    bool              error_read;
};


#endif //PROTOS_EXTCMD_H
