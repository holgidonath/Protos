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
typedef enum filterState {
    FILTER_CLOSE,
    FILTER_ENDING,
    FILTER_ALL_SENT,
    FILTER_STARTING,
    FILTER_FILTERING,
} filterState;

struct extern_cmd {
    int                     pipe_in[2];
    int                     pipe_out[2];
    filterState             state;
};

#endif //PROTOS_EXTCMD_H
