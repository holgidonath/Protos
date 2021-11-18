#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>

typedef struct metrics * metrics_t;

struct metrics {
	uint32_t current_connections;
	uint32_t total_connections;
	uint32_t total_bytes;
	char * management_addr;
	uint16_t managment_port;
};

metrics_t metrics(void);
void free_metrics(metrics_t metrics);

#endif