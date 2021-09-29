#include <stdlib.h>
#define BUFFSIZE 1024
struct buffer {
	char * buffer;
	size_t len;     // longitud del buffer
	size_t from;    // desde donde falta escribir
};