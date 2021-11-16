#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <sys/select.h>

#include "parser_admin.h"
#include "thread_admin.h"
#include "socket_admin.h"

int create_socket(void);
int bind_admin(int admin_fd, struct sockaddr_in * admin_addr, size_t admin_addr_size);
int set_socket_options(int admin_fd, int level, int option_name, void * option_value, socklen_t option_length);
int listen_admin(int admin_fd, int max_connections);

int init_socket(struct sockaddr_in * admin_addr, socklen_t * admin_addr_length) {
	int admin_fd;
	struct sctp_initmsg init_msg;

	memset(admin_addr, 0, sizeof(*admin_addr));
	admin_fd = create_socket();

	admin_addr->sin_family = AF_INET;
	if(strcmp(metrics->managment_addr, "loopback") == 0) {
		admin_addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	} else if(strcmp(metrics->managment_addr, "any") == 0) {
		admin_addr->sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		inet_pton(AF_INET, metrics->managment_addr, &(admin_addr->sin_addr));
	}

	admin_addr->sin_port = htons(metrics->managment_port);
	bind_admin(admin_fd, admin_addr, sizeof(*admin_addr));
	memset(&init_msg, 0, sizeof(init_msg));
	init_msg.sinit_num_ostreams = MAX_STREAMS;
	init_msg.sinit_max_instreams = MAX_STREAMS;
	init_msg.sinit_max_attempts = MAX_ATTEMPTS;
	set_socket_options(admin_fd, IPPROTO_SCTP, SCTP_INITMSG, &init_msg, sizeof(init_msg));
	listen(admin_fd, MAX_CONNECTIONS);

	return admin_fd;
}

int create_socket(void) {
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	if(fd == -1) {
		perror("Error creating admin socket");
		exit(EXIT_FAILURE);
	}
	return fd;
}

int bind_admin(int admin_fd, struct sockaddr_in * admin_addr, size_t admin_addr_size) {
	int fd = bind(admin_fd, (struct sockaddr *)admin_addr, admin_addr_size);
	if(fd == -1) {
		perror("Error binding admin");
		close(admin_fd);
		exit(EXIT_FAILURE);
	}
	return fd;
}

int set_socket_options(int admin_fd, int level, int option_name, void * option_value, socklen_t option_length) {
	int fd = setsockopt(admin_fd, level, option_name, option_value, option_length);
	if(fd == -1) {
		perror("Error setting admin socket options");
		close(admin_fd);
		exit(EXIT_FAILURE);
	}
	return fd;
}

int listen_admin(int admin_fd, int max_connections) {
	int fd = listen(admin_fd, max_connections);
	if(fd == -1) {
		perror("Error listenning admin");
		close(admin_fd);
		exit(EXIT_FAILURE);
	}
	return fd;
}

void resolve_admin_client(int admin_fd, fd_set * read_fds, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics) {
	if(FD_ISSET(admin_fd, read_fds)) {
		resolve_admin_fd_thread(admin_fd, admin_addr, admin_addr_length, metrics);
	}
}

void resolve_sctp_client(int admin_fd, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics) {
	int ret = -1;
	int fd = -1;
	char request[BUFFER_MAX];
	int request_length = -1;
	char response[BUFFER_MAX];
	int response_length = -1;
	bool logged = false;
	bool stop = false;

	fd = accept(admin_fd, (struct sockaddr *)admin_addr, admin_addr_length);
	if(fd == -1) {
		perror("Error accepting admin");
		return;
	}
	ok(response, &response_length);
	ret = sctp_sendmsg(fd, (void *)response, response_length, NULL, 0, 0, 0, 0, 0, 0);
	if(ret == -1 || ret == 0) {
		perror("Error sending admin response");
		stop = true;
	} else {
		log_message(false, "Successfully sent response to admin");
	}
	while(!stop) {
		request_length = sctp_recvmsg(fd, request, BUFFER_MAX, NULL, 0, 0, 0);
		if(request_length == -1 || request_length == 0) {
			perror("Error receiving admin request");
			break;
		} else {
			log_message(false, "Successfully received request from admin");
			stop = commnads(&logged, request, request_length, response, &response_length, settings, metrics);
		}

		if(request_length != 0) {
			ret = sctp_sendmsg(fd, (void *)response, response_length, NULL, 0, 0, 0, 0, 0, 0);
			if(ret == -1 || ret == 0) {
				perror("Error sending admin response");
				break;
			} else {
				log_message(false, "Successfully sent response to admin");
			}
		}
	}
	log_message(false, "Closing admin_fd");
	close(fd);
}