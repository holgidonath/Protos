#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

typedef struct metrics * metrics_t;

struct metrics {
    uint32_t concurrent_connections;
    uint32_t total_connections;
    uint32_t bytes_transfered;
};

metrics_t
init_metrics(void);

void
free_metrics(metrics_t metrics);


#endif
