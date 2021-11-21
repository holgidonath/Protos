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

int get_authentication(int sock,char * in_buffer,char * out_buffer)
{
    int n;
    int nr;
    fgets(out_buffer, BUFF_SIZE, stdin);
    strtok(out_buffer, "\n");
    if (n = sctp_sendmsg(sock, out_buffer, strlen(out_buffer),
                         NULL, 0, 0, 0, 0, 0, 0) < 0) {
        printf("ERROR\n");
        return -1;
    }
    nr = sctp_recvmsg(sock, in_buffer, BUFF_SIZE, NULL, 0, 0, 0);
    if (nr <= 0) {
        printf("ERROR\n");
        return -1;
    }
    if (in_buffer[0] == '+')
    {
        return  1;
    }
    else
    {
        return 0;
    }

}


int main(const int argc, char **argv) {

    int status = 0;
    struct admin_opt opt;
    struct address_data addr;
    printf("hola\n");
    char out[BUFF_SIZE] = {0};
    char incoming[BUFF_SIZE] = {0};
    parse_admin_options(argc, argv, &opt);
    set_mgmt_address(&addr, opt.mgmt_addr, &opt);
    int sock = socket(addr.mgmt_domain, SOCK_STREAM, IPPROTO_SCTP);
    printf("hola\n");
    if(sock < 0)
    {
        printf("Failed to create socket\n");
        close(sock);
    }
    int con = connect(sock, (struct sockaddr*)&addr.mgmt_addr, addr.mgmt_addr_len);
    if (con < 0)
    {
        printf("Failed to connect to management\n");
        close(sock);
    }
    int n = sctp_recvmsg(sock, incoming, BUFF_SIZE, NULL, 0,0,0);
    printf(incoming);
    printf("\n");
    if(n < 0)
    {
        printf("Error getting greeting\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    while(status == 0);
    {
        printf("Please enter Password to enter. You may type 'QUIT to exit Password: \n");
        status = get_authentication(sock,incoming, out);

    }
    return 0;
}














