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

static void
filter_destroy(struct selector_key *key);
/* ==================================================== */
/*                   IMPLEMENTATION                     */
/* ==================================================== */
void
env_var_init(char *username) {
    char env_pop3filter_version[30];
    char env_pop3_server[150];
    char env_pop3_username[30];
    struct opt * opt = get_opt();
    sprintf(env_pop3filter_version, "POP3FILTER_VERSION=%s", VERSION);
    if(putenv(env_pop3filter_version)) {
        log(ERROR, "putenv() couldn't create %s environment variable", env_pop3filter_version);
    }

    sprintf(env_pop3_server, "POP3_SERVER=%s", opt->origin_server);
    if(putenv(env_pop3_server)) {
        log(ERROR, "putenv() couldn't create %s environment variable", env_pop3_server);
    }

    sprintf(env_pop3_server, "POP3_USERNAME=%s", username);
    if(putenv(env_pop3_username)) {
        log(ERROR, "putenv() couldn't create %s environment variable", username);
    }
}

int
read_from_filter(struct selector_key *key) {
    log(INFO, "read_from_filter: Reading from filter...");
    unsigned ret = COPY;
    int fd = key->fd;
    struct connection  * conn = ATTACHMENT(key);
    struct copy * copy  = get_copy_ptr(key);
    struct opt * opt = get_opt();
    size_t size;
    buffer * buffer = copy->read_buffer;
    uint8_t *ptr = buffer_write_ptr(buffer, &size);

    bool interest_retr = opt->cmd ? true : false;
    log(DEBUG, "read_from_filter: opt->cmd exist: %s", opt->cmd);

    ssize_t n = read(fd, ptr, size);
    if( n > 0 ) {
        // TODO update metrics
        buffer_write_adv(b, n)
        log(INFO, "read_from_filter: Coppied %zd bytes from filter to proxy", n);

    } else if( n == 0 ) {
        log(DEBUG, "read_from_filter: Filter EOF.");
        filter_destroy(key);
        // TODO aca que onda?
        if(!buffer_can_read(buffer) && !buffer_can_write(conn->write_buffer)) {
            copy->duplex = OP_NOOP;
        }

    } else {
        log(FATAL, "read_from_filter: Filter broken >,<");
        conn->data_filter.state = FILTER_ENDING;
    }

    return ret;
}

int
write_buffer_to_filter(struct selector_key *key, buffer* buff) {
    struct extern_cmd * filter = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
    buffer *sb = filter->wb;
    uint8_t *b;
    size_t count;
    ssize_t n = 0;
    size_t i = 0;

    char c;
    while (buffer_can_read(buff) && n == 0) {
        c = buffer_read(buff);
        i++;
        if (c == '\n') {
            n = i;
        }
        buffer_write(sb, c);
    }

    if (n == 0) {
        return 0;
    }

    b = buffer_read_ptr(sb, &count);
    n = write(ATTACHMENT(key)->w_to_filter_fds[WRITE], b, count);
    buffer_reset(sb);

    return n;
}

static void
filter_destroy(struct selector_key *key) {
    struct connnection *  conn    = ATTACHMENT(key);
    struct data_filter * data_filter   = &conn->data_filter;

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
        if(filter_data->fdout[i] > 0) {
            selector_unregister_fd(key->s, data_filter->fdout[i]);
            close(data_filter->fdout[i]);
        }
    }
    memset(data_filter, 0, sizeof(struct data_filter));

    data_filter->state = FILTER_CLOSE;
    log(DEBUG, "filter_destroy: FILTER_CLOSE");
}
