#include <stdint.h>
#include <stdio.h>
#include "include/extcmd.h"
#include "include/args.h"
#include "include/selector.h"
#include "include/main.h"
#include "include/logger.h"
#include "include/proxytcp.h"

/* ==================================================== */
/*                     PROTOTYPES                       */
/* ==================================================== */

int
read_from_filter(struct selector_key *key);

void
env_var_init(char *username);

/* ==================================================== */
/*                   IMPLEMENTATION                     */
/* ==================================================== */
void
env_var_init(char *username) { // envvar_buffer es global definido en args.h
    struct opt * opt = get_opt();
    sprintf(envvar_buffer, "POP3FILTER_VERSION=%s", VERSION);
    if(putenv(envvar_buffer)) {
        log(ERROR, "env_var_init: couldn't create 'POP3FILTER_VERSION=%s' environment variable", VERSION);
    }

    sprintf(envvar_buffer, "POP3_SERVER=%s", opt->origin_server);
    if(putenv(envvar_buffer)) {
        log(ERROR, "env_var_init: couldn't create 'POP3_SERVER=%s' environment variable", opt->origin_server);
    }

    sprintf(envvar_buffer, "POP3_USERNAME=%s", username);
    if(putenv(envvar_buffer)) {
        log(ERROR, "env_var_init: couldn't create 'POP3_USERNAME=%s' environment variable", username);
    }
}

void
filter_destroy(struct selector_key *key) {
    struct connection *  conn        = ATTACHMENT(key);
    struct data_filter * data_filter  = &conn->data_filter;

    if(data_filter->pid_child > 0) {
        log(INFO, "filter_destroy: Destroying process %ld", (long)data_filter->pid_child);
        kill(data_filter->pid_child, SIGKILL);
    }
    else if(data_filter->pid_child == -1) {
        log(ERROR, "filter_destroy: Process PID = %ld returned", (long)data_filter->pid_child);
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 2; i++) {
        if(data_filter->fdin[i] > 0) {
            selector_unregister_fd(key->s, data_filter->fdin[i]);
            close(data_filter->fdin[i]);
        }
        if(data_filter->fdout[i] > 0) {
            selector_unregister_fd(key->s, data_filter->fdout[i]);
            close(data_filter->fdout[i]);
        }
    }
    memset(data_filter, 0, sizeof(struct data_filter));

    data_filter->state = FILTER_CLOSE;
    log(DEBUG, "filter_destroy: FILTER_CLOSE");
}
