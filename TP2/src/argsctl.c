#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include <assert.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "include/argsctl.h"
#include "include/logger.h"


void
admin_usage() {
    printf(
            "pop3ctl [-h] [-L <management-address>] [-o <management-port>]\n"
    );
}

void
help_admin() {
    printf(
            "USAGE\n"
            "    pop3ctl [OPTIONS]\n"
            "\n"
            "ARGUMENTS\n"
            "-h\n"
            "    Prints help\n"
            "\n"
            "-L <management-address>\n"
            "    Specifies address where management server is listening.\n"
            "    Default using loopback\n"
            "\n"
            "-o <management-port>\n"
            "    Specifies port where management server is listening.\n"
            "    Default is 9090\n"
            "\n"
    );

    exit( EXIT_SUCCESS );
}

static unsigned short
port(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        exit(1);

    }
    return (unsigned short)sl;
}

void
parse_admin_options(int argc, char **argv, struct admin_opt * opt) {
    /* Setting default values  */
    assert(argv && opt);
    memset(opt, 0, sizeof(*opt));
    opt->mgmt_port    = 9090;     /* TODO:default value */
    opt->mgmt_addr    = "127.0.0.1";

    /* Parse command line arguments */
    int c;
    const char *opts = "L:o:h";
    while ((c = getopt(argc, argv, opts)) != -1) {
        switch (c) {
            case 'h':
                help_admin();
                break;
            case 'o':
                opt->mgmt_port = port(optarg);
                break;
            case 'L':
                opt->mgmt_addr = optarg;
                break;
            case '?':
                admin_usage();
                printf("Invalid Arguments");
                exit(1);


        }
    }

//    if( argc < 2 ) {
//        admin_usage();
//        exit( EXIT_FAILURE );
//    }
}

void
set_mgmt_address(struct address_data * address_data, const char * adress, struct admin_opt * opt) {
    memset(&(address_data->mgmt_addr), 0, sizeof(address_data->mgmt_addr));

    address_data->mgmt_type = ADDR_IPV4;
    address_data->mgmt_domain  = AF_INET;
    address_data->mgmt_addr_len = sizeof(struct sockaddr_in);


    struct sockaddr_in ipv4;
    memset(&(ipv4), 0, sizeof(ipv4));
    ipv4.sin_family = AF_INET;
    int result = 0;

    if((result = inet_pton(AF_INET, adress, &ipv4.sin_addr.s_addr)) <= 0) {
        address_data->mgmt_type   = ADDR_IPV6;
        address_data->mgmt_domain  = AF_INET6;
        address_data->mgmt_addr_len = sizeof(struct sockaddr_in6);


        struct sockaddr_in6 ipv6;

        memset(&(ipv6), 0, sizeof(ipv6));

        ipv6.sin6_family = AF_INET6;

        ipv6.sin6_port = htons(opt->mgmt_port);
        memcpy(&address_data->mgmt_addr, &ipv6, address_data->mgmt_addr_len);
        return;
    }
    ipv4.sin_port = htons(opt->mgmt_port);
    memcpy(&address_data->mgmt_addr, &ipv4, address_data->mgmt_addr_len);
    return;
}
