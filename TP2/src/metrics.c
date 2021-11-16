#include <stdio.h>
#include <stdlib.h>

#include "metrics.h"

metrics_t metrics(void) {
	metrics_t metrics = (metrics_t) malloc(sizeof(*metrics));
	if(metrics == NULL) {
	perror("Error with metrics");
	exit(EXIT_FAILURE);
	}
	return metrics;
}

void free_metrics(metrics_t metrics) {
	free(metrics);
}