#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include "logger.h"
#include <unistd.h>

const char *appname;

static unsigned short
port(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s ||
        '\0' != *end ||
        ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno) ||
        sl < 0 ||
        sl > USHRT_MAX) {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        exit( EXIT_FAILURE );
        return 1;
    }

    return (unsigned short)sl;
}

/* help */
static void
help() {
    printf(
            "USAGE\n"
            "    %s [OPTIONS] origin-server\n"
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
            "-h\n"
            "    Prints help\n"
            "-l <pop3-address>\n"
            "    Set proxy address.\n"
            "    Default listening at all interfaces\n"
            "-L <management-address>\n"
            "    Set management service address.\n"
            "    Default using loopback\n"
            "-o <management-port>\n"
            "    Set port for management service.\n"
            "    Default is 9090\n"
            "-p <local-port>\n"
            "    TCP port for incoming POP3 connections\n"
            "    Default is 1110\n"
            "-P <origin-port>\n"
            "    TCP origin port for POP3 origin server\n"
            "    Default is 110\n"
            "-t <cmd>\n"
            "    Command for extern filters.\n"
            "-v\n"
            "    Prints version related information\n"
            "\n"
            , appname);

    exit( EXIT_SUCCESS );
}

static void
version() {
    printf("%s %s\n", appname, VERSION);

    exit( EXIT_SUCCESS );
}


static void
usage(){
    printf(
            "%s [-hv] [-e <file>] [-l <pop3-address>] [-L <management-address>] [-o <management-port>] [-p <local-port>] [-P <origin-port>] [-t <cmd>] origin-server\n"
            , appname);
}


void parseOptions(int argc, char **argv, struct opt *opt) {
    /* Setting default values  */
    assert(argv && opt);
    memset(opt, 0, sizeof(*opt));
    opt->local_port   = 1110;
    opt->mgmt_port    = 9090;
    opt->origin_port  = 110;
    opt->fstderr      = "/dev/null";
    opt->pop3_addr    = NULL;        /* TODO:default value */
    opt->mgmt_addr    = NULL;        /* TODO:default value */

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
                opt->exec = optarg;
                break;
            case 'e':
                opt->fstderr = optarg;
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


int main(int argc, char **argv) {
    appname = *argv;
    struct opt opt;
    parseOptions(argc, argv, &opt);
    /* print options just for debug */
    printf("fstderr       = %s\n", opt.fstderr);
    printf("local_port    = %d\n", opt.local_port);
    printf("origin_port   = %d\n", opt.origin_port);
    printf("mgmt_port     = %d\n", opt.mgmt_port);
    printf("mgmt_addr     = %s\n", opt.mgmt_addr);
    printf("pop3_addr     = %s\n", opt.pop3_addr);
    printf("origin_server = %s\n", opt.origin_server);
    printf("cmd           = %s\n", opt.exec);


    // TODO: basic proxy
}