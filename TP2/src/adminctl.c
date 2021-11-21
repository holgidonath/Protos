#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/sctp.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "include/argsctl.h"
#include "include/logger.h"

int main(const int argc, char **argv) {
    struct admin_opt opt;
    struct address_data addr;
    char out[BUFF_SIZE];
    char incoming[BUFF_SIZE];
    parse_admin_options(argc, argv, &opt);
    set_mgmt_address(&addr, opt.mgmt_addr);
    int sock = socket(addr.mgmt_domain, SOCK_STREAM, IPPROTO_SCTP);
    if(sock < 0)
    {
        log(ERROR, "Failed to create socket");
        close(sock);
    }
    int con = connect(sock, (struct sockaddr*)addr.mgmt_addr, addr.mgmt_addr_len);
    if (con < 0)
    {
        log(ERROR, "Failed to connect to management");
        close(sock);
    }
    int n = sctp_recvmsg(sock, incoming, BUFF_SIZE, NULL, 0,0,0);
    if(n < 0)
    {
        printf("Error getting greeting\n");
        close(sock);
        exit(EXIT_FAILURE);
    }




    return 0;
}

