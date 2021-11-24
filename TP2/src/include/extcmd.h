#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include "buffer.h"
#include <stdbool.h>
#include "selector.h"
#include <signal.h>
#include <fcntl.h>

#define READ 0
#define WRITE 1
#define CHILD_BUFFER_SIZE 2048

/* ==================================================== */
/*                     PROTOTYPES                       */
/* ==================================================== */
void
env_var_init(char *username);

void
filter_destroy(struct selector_key *key);

/**
 * Posibles estados del filtro.
 */
typedef enum filter_status {
    FILTER_CLOSE,
    FILTER_ENDING,
    FILTER_ALL_SENT,
    FILTER_STARTING,
    FILTER_FILTERING,
} filter_status;

typedef struct data_filter {
    int                     fdin[2];
    int                     fdout[2];
    filter_status           state;
    pid_t                   pid_child;
} data_filter;

#endif //PROTOS_EXTCMD_H
