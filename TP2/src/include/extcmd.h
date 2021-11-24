#ifndef PROTOS_EXTCMD_H
#define PROTOS_EXTCMD_H

#include "buffer.h"
#include <stdbool.h>
#include "selector.h"

#define READ 0
#define WRITE 1

void env_var_init(char *username);
void worker_secondary(struct selector_key *key);

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
    pid_t                   pid_child
}; data_filter

#endif //PROTOS_EXTCMD_H
