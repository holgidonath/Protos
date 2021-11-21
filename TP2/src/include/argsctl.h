#ifndef PROTOS_ARGSCTL_H
#define PROTOS_ARGSCTL_H

#include <sys/socket.h>

#define BUFF_SIZE 2048

typedef enum address_type {
    ADDR_IPV4   = 0x01,
    ADDR_IPV6   = 0x02,
    ADDR_DOMAIN = 0x03,
} address_type;


struct admin_opt {
    short mgmt_port;
    char * mgmt_addr;
};
typedef struct address_data {

    u_int8_t mgmt_port;
    struct sockaddr_storage mgmt_addr;
    address_type mgmt_type;
    socklen_t mgmt_addr_len;
    int mgmt_domain;

} address_data;


void parse_admin_options(int argc, char **argv, struct admin_opt *opt);
void set_mgmt_address(struct address_data * address_data, const char * adress, struct admin_opt * opt);
#endif //PROTOS_ARGSCTL_H

