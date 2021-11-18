#ifndef THREAD_ADMIN_H
#define THREAD_ADMIN_H

void resolve_admin_fd_thread(int admin_fd, struct sockaddr_in * admin_addr, socklen_t * admin_addr_length, metrics_t metrics);

#endif