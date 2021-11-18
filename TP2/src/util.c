#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#include "include/util.h"

void *safe_malloc(size_t bytes) {
    void *p;
    if ( (p = malloc(bytes)) == NULL ){
        fprintf(stderr, "malloc can't allocate %lu bytes\n", (unsigned long)bytes);
        exit(EXIT_FAILURE);
    }
    return p;
}