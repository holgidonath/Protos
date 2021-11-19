#include "include/extcmd.h"
#include "include/buffer.h""
#include "include/selector.h"
#include "include/main.h"


void env_var_init(char *username) {
    sprintf(env_pop3filter_version, "POP3FILTER_VERSION=", VERSION);
    if(putenv(env_pop3filter_version)) {
        log(ERROR, "putenv() couldn't create %s environment variable", env_pop3filter_version);
    }

    sprintf(env_pop3_server, "POP3_SERVER=", opt.origin_server);
    if(putenv(env_pop3_server)) {
        log(ERROR, "putenv() couldn't create %s environment variable", env_pop3_server);
    }

    sprintf(env_pop3_server, "POP3_USERNAME=", opt.origin_server);
    if(putenv(env_pop3_username)) {
        log(ERROR, "putenv() couldn't create %s environment variable", username);
    }
}

enum extern_cmd_status
add_to_selector(struct selector_key * key, int pipe_out[2], int pipe_in[2]) {
    struct pop3 * data = ATTACHMENT(key);

    if (selector_register(key->s, pipe_out[READ], &cmd_handler, OP_READ, data) == 0 &&
        selector_fd_set_nio(pipe_out[READ]) == 0) {
        data->cmd_read_fd = pipe_out[READ];
    } else {
        close(pipe_out[READ]);
        close(pipe_in[WRITE]);
        return CMD_STATUS_ERROR;
    }

    if (selector_register(key->s, pipe_in[WRITE], &cmd_handler, OP_WRITE, data) == 0 &&
        selector_fd_set_nio(pipe_in[WRITE]) == 0) {
        data->cmd_write_fd = pipe_in[WRITE];
    } else {
        selector_unregister_fd(key->s, pipe_in[1]);
        close(pipe_out[READ]);
        close(pipe_in[WRITE]);
        return CMD_STATUS_ERROR;
    }

    return CMD_STATUS_DONE;
}

/* Forward data from socket 'source' to socket 'destination' by executing the 'cmd' command */
enum extern_cmd_status
socket_forwarding_cmd (struct selector_key * key, char *cmd) {
    int n, i;
    const size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];

    // pipe_in : father --> child
    // pipe_out: child  --> father
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) { // create command input and output pipes
        log(ERROR, "socket_forwarding_cmd: Cannot create pipe");
        return CMD_STATUS_ERROR
    }

    pid_t pid = fork();
    if( pid == 0) {
        dup2(pipe_in[READ], STDIN_FILENO); // stdin --> pipe_in[READ]
        dup2(pipe_out[WRITE], STDOUT_FILENO); // stdout --> pipe_out[WRITE]
        close(pipe_in[WRITE]);
        close(pipe_out[READ]);
        log(INFO, "socket_forwarding_cmd: executing command");
        n = system(cmd);
        log(DEBUG, "socket_forwarding_cmd: BACK from executing command");
        _exit(n);
    } else {
        close(pipe_in[READ]);
        close(pipe_out[WRITE]);

        if (add_to_selector(key, pipe_out, pipe_in) == CMD_STATUS_ERROR) {
            log(ERROR, "socket_forwarding_cmd: ")
            return CMD_STATUS_ERROR;
        }
//        while ((n = recv(source, buffer, BUFFER_SIZE, 0)) > 0) { // read source socket --> write to buffer
//            if (write(pipe_in[WRITE], buffer, n) < 0) {
//                log(ERROR, "socket_forwarding_cmd: Cannot write to input pipe of external command");
//                exit(EXIT_FAILURE);
//            }
//            else {
//                log(INFO, "socket_forwarding_cmd: writing to input pipe of external command");
//            }
//            if ((i = read(pipe_out[READ], buffer, BUFFER_SIZE)) > 0) {
//                log(INFO, "socket_forwarding_cmd: sending command output to destination socket...")
//                send(destination, buffer, i, 0);
//            }
//        }

        return CMD_STATUS_OK;
    }
}

void
cmd_close(struct selector_key * key) {
    // TODO algo mas aca??
    close(key->fd);
}

void
cmd_write(struct selector_key * key) {
    struct extern_cmd *extern_cmd  = &ATTACHMENT(key)->extern_cmd;

    buffer  *buff = extern_cmd->cmd_wb;
    uint8_t *ptr;
    size_t   count;
    ssize_t  n;

    ptr = buffer_read_ptr(buff, &count);
    size_t bytes_sent = count;

    if (extern_cmd->send_bytes_read != 0){
        bytes_sent = extern_cmd->send_bytes_read;
    }

    n = write(*extern_cmd->write_fd, ptr, bytes_sent);
    if (n > 0) {
        if (extern_cmd->send_bytes_read != 0) {
            extern_cmd->send_bytes_read -= n;
        }
        buffer_read_adv(buff, n);

        if (extern_cmd->done_read && extern_cmd->send_bytes_read == 0) {
            selector_unregister_fd(key->s, key->fd);

        } else {
            selector_set_interest(key->s, *extern_cmd->write_fd, OP_NOOP);
            selector_set_interest(key->s, *extern_cmd->origin_fd, OP_READ);
        }

    } else if (n == -1) {
        extern_cmd->status = CMD_STATUS_ERROR;
        if (extern_cmd->send_bytes_read == 0) {
            buffer_reset(buff);

        } else {
            buffer_read_adv(buff, extern_cmd->send_bytes_read);
        }

        selector_unregister_fd(key->s, key->fd);
        selector_set_interest(key->s, *extern_cmd->origin_fd, OP_READ);
        extern_cmd->error_read = true;
    }
}

void cmd_read(struct selector_key * key) {
    uint8_t * ptr;
    size_t    count;
    ssize_t   n;

    struct extern_cmd * extern_cmd  = &ATTACHMENT(key)->extern_cmd;
    buffer * buff  = extern_cmd->cmd_rb;
    ptr = buffer_write_ptr(buff, &count);

    n   = read(*extern_cmd->read_fd, ptr, count);
    if (n < 0) {
        selector_unregister_fd(key->s, key->fd);
        extern_cmd->error_write = true;
        selector_set_interest(key->s, *extern_cmd->client_fd, OP_WRITE);

    } else if (n >= 0) {
        buffer_write_adv(buff, n);

        // TODO Parsing mail??

        selector_set_interest(key->s, *extern_cmd->client_fd, OP_WRITE);
    }
}
