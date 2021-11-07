/**

 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "socks5.h"
#include "../TP2/src/selector.h"
#include "socks5nio.h"

#include "include/main.h"
#include "include/logger.h"
#include "include/args.h"

static bool done = false;
const char *appname;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int
main(const int argc, const char **argv) {
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

}
