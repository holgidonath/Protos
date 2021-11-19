#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "include/socket_admin.h"
#include "include/metrics.h"
#include "include/thread_admin.h"

typedef struct thread_args * thread_args_t

struct thread_args {
	pthread_t p_id;
	int admin_fd;
	struct sockaddr_in * admin_addr;
	socklen_t * admin_addr_length;
	metrics_t metrics;
};

static void * resolve_admin_thread(void * args);

void resolve_admin_fd_thread(int admin_fd, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics) {
	thread_args_t thread_args = malloc(sizeof(*thread_args));
	if(thread_args == NULL) {
		perror("Error creating admin thread");
		exit(EXIT_FAILURE);
	}
	thread_args->p_id = pthread_self();
	thread_args->admin_fd = admin_fd;
	thread_args->admin_addr = admin_addr;
	thread_args->admin_addr_length = admin_addr_length;
	thread_args->metrics = metrics;

	pthread_t thread;
	if(pthread_create(&thread, NULL, resolve_admin_thread, (void *)thread_args) == -1) {
		perror("Error creating admin thread");
		exit(EXIT_FAILURE);
	}
}

stactic void * resolve_admin_thread(void * args) {
	thread_args_t thread_args = (thread_args_t) args;
	resolve_sctp_client(thread_args->admin_fd, thread_args->admin_addr, thread_args->admin_addr_length, thread_args->metrics);
	free(thread_args);
	pthread_detach(pthread_self());
	pthread_exit(NULL);
	return NULL;
}