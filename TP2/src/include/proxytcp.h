#ifndef PROTOS_PROXYTCP_H
#define PROTOS_PROXYTCP_H

#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include "stm.h"
#include "proxyadmin.h"
#include "extcmd.h"
#include "args.h"
#include "buffer.h"
#include "selector.h"

#define N(x)                (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key)     ( ( struct connection * )(key)->data)

/* ==================================================== */
/*                     PROTOTYPES                       */
/* ==================================================== */
struct opt * get_opt(void);
char * get_stats(void);

/* ==================================================== */
/*                     STATES                           */
/* ==================================================== */
enum proxy_states
{
    RESOLVE_ORIGIN = 0,
    CONNECT,
    FILTER,
    COPY,
    DONE,
    PERROR,
};

typedef enum address_type {
    ADDR_IPV4   = 0x01,
    ADDR_IPV6   = 0x02,
    ADDR_DOMAIN = 0x03,
} address_type;

/* ==================================================== */
/*                     STRUCTURES                       */
/* ==================================================== */
/* ----------- COPY -------- */

/* --------- ADDRESS  -------- */
typedef union address {
    char                    fqdn[0xFF];
    struct sockaddr_storage addr_storage;
} address;

typedef struct address_data {

    in_port_t origin_port;
    address origin_addr;
    address_type origin_type;
    socklen_t origin_addr_len;
    int origin_domain;

} address_data;

/* --------- CONNECTION  -------- */
struct connection
{
    int client_fd;
    struct copy copy_client;
    int origin_fd;
    struct address_data origin_data;
    struct addrinfo *origin_resolution;
    struct addrinfo *origin_resolution_current;
    struct copy copy_origin;
    buffer read_buffer, write_buffer, filter_buffer;
    uint8_t raw_buff_a[2048], raw_buff_b[2048], raw_buff_c[2048];

    struct state_machine stm;
    struct connection * next;
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    unsigned                references;
    bool                    was_greeted;

    /* External Process */

    struct extern_cmd    extern_cmd;

    int                     infd;
    int                     outfd;
};


#endif
