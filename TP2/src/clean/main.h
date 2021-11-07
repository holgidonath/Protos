#ifndef _MAIN_H_
#define _MAIN_H_

#ifndef VERSION
#define VERSION  "0.0.1"
#endif

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

#endif