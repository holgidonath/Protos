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

struct socks5args {
    char           *socks_addr;
    unsigned short  socks_port;

    char *          mng_addr;
    unsigned short  mng_port;

    bool            disectors_enabled;

    struct doh      doh;
    struct users    users[MAX_USERS];
};

struct opt{
    short local_port;     /* local port */
    short mgmt_port;      /* management port */
    short origin_port;    /* POP3 origin server port */
    const char *fstderr;	/* stderr file */
    char *exec;		        /* filter */
    char *pop3_addr;      /* proxy listen address */
    char *mgmt_addr;      /* management address */
    char *origin_server;  /* POP3 origin server address */
};


/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct socks5args *args);

#endif
