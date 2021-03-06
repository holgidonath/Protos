#ifndef SOCKET_ADMIN_H
#define SOCKET_ADMIN_H

#include <stdbool.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "metrics.h"

#define BUFFER_MAX 2048
#define MAX_CONNECTIONS 1
#define MAX_STREAMS 1
#define MAX_ATTEMPTS 5
#define MAX_INT_DIGITS 10

enum requests {
	LOGIN = 0,
	LOGOUT,
	GET_CURRENT_CONNECTIONS,
	GET_TOTAL_CONNECTIONS,
	GET_TOTAL_BYTES
};

enum responses {
	OK = 0,
	ERR
};

int init_socket(struct sockaddr_in * admin_addr, socklen_t * admin_addr_length);
void resolve_admin_client(int admin_fd, fd_set * read_fds, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics);
void resolve_sctp_client(int admin_fd, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics);

#endif