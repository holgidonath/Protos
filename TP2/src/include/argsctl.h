#ifndef PROTOS_ARGSCTL_H
#define PROTOS_ARGSCTL_H

#define BUFF_SIZE 2048

struct admin_opt {
    short mgmt_port;
    char * mgmt_addr;
};
typedef struct address_data {

    in_port_t mgmt_port;
    struct sockaddr_storage mgmt_addr;
    address_type mgmt_type;
    socklen_t mgmt_addr_len;
    int mgmt_domain;

} address_data;

void parse_admin_options(int argc, char **argv, struct admin_opt *opt);
void set_mgmt_address(struct address_data * address_data, const char * adress);
#endif //PROTOS_ARGSCTL_H
