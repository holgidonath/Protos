#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10

struct users {
    char *name;
    char *pass;
};

struct doh {
    char           *host;
    char           *ip;
    unsigned short  port;
    char           *path;
    char           *query;
};

/* Commandline arguments */
struct opt {
    short local_port;            /* local port */
    short mgmt_port;             /* management port */
    short origin_port;           /* POP3 origin server port */
    const char *fstderr;	     /* stderr file */
    char *cmd;		             /* filter */
    char *pop3_addr;             /* proxy listen address */
    char *mgmt_addr;             /* management address */
    char *origin_server;         /* POP3 origin server address */
};

/* Environment variables */
static char env_pop3filter_version[15];
static char env_pop3_server[135];
static char env_pop3_username[25];

#endif

