#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include <assert.h>

#include "include/args.h"
#include "include/logger.h"
#include "include/main.h"

static unsigned short
port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}

static void
user(char *s, struct users *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }

}

/* help */
void
help() {
    printf(
            "USAGE\n"
            "    pop3filter [OPTIONS] origin-server\n"
            "\n"
            "ARGUMENTS\n"
            "<origin-server>\n"
            "    POP3 origin server address.\n"
            "    IPV4, IPV6, Domain Name\n"
            "\n"
            "OPTIONS\n"
            "-e <error-file>\n"
            "    Set error file to redirec stderr.\n"
            "    Default is /dev/null\n"
            "\n"
            "-h\n"
            "    Prints help\n"
            "\n"
            "-l <pop3-address>\n"
            "    Set proxy address.\n"
            "    Default listening at all interfaces\n"
            "\n"
            "-L <management-address>\n"
            "    Set management service address.\n"
            "    Default using loopback\n"
            "\n"
            "-o <management-port>\n"
            "    Set port for management service.\n"
            "    Default is 9090\n"
            "\n"
            "-p <local-port>\n"
            "    TCP port for incoming POP3 connections\n"
            "    Default is 1110\n"
            "\n"
            "-P <origin-port>\n"
            "    TCP origin port for POP3 origin server\n"
            "    Default is 110\n"
            "\n"
            "-t <cmd>\n"
            "    Command for extern filters.\n"
            "\n"
            "-v\n"
            "    Prints version related information\n"
            "\n"
            );

    exit( EXIT_SUCCESS );
}



void
version() {
    printf("%s %s\n", "pop3filter", VERSION);
    exit( EXIT_SUCCESS );
}

void
usage(){
    printf(
            "pop3filter [-hv] [-e <file>] [-l <pop3-address>] [-L <management-address>] [-o <management-port>] [-p <local-port>] [-P <origin-port>] [-t <cmd>] origin-server\n"
            );
}



void
parse_options(int argc, char **argv, struct opt * opt) {
    /* Setting default values  */
    assert(argv && opt);
    memset(opt, 0, sizeof(*opt));
    opt->local_port   = 1110;
    opt->mgmt_port    = 9090;
    opt->origin_port  = 110;
    opt->fstderr      = "/dev/null";
    opt->pop3_addr    = NULL;        /* TODO:default value */
    opt->mgmt_addr    = NULL;        /* TODO:default value */
    opt->cmd          = NULL;

    /* Parse command line arguments */
    int c;
    const char *opts = "e:l:L:o:p:P:t:hv";
    while ((c = getopt(argc, argv, opts)) != -1) {
        switch (c) {
            case 'h':
                help();
                break;
            case 'v':
                version();
                break;
            case 'o':
                opt->mgmt_port = port(optarg);
                break;
            case 'p':
                opt->local_port = port(optarg);
                break;
            case 'P':
                opt->origin_port = port(optarg);
                break;
            case 'l':
                opt->pop3_addr = optarg;
                break;
            case 'L':
                opt->mgmt_addr = optarg;
                break;
            case 't':
                opt->cmd = optarg;
                break;
            case 'e':
                opt->fstderr = optarg;
                freopen(opt->fstderr,"w",stderr);
                break;
            case '?':
                log(FATAL, "Invalid arguments");
                exit( EXIT_FAILURE );
        }
    }

    if( argc < 2 ) {
        usage();
        exit( EXIT_FAILURE );
    }
    opt->origin_server = argv[optind];
}





