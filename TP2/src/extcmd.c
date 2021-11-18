#include "include/extcmd.h"

// TODO: Manejo de estados y todo eso

/* Forward data from socket 'source' to socket 'destination' by executing the 'cmd' command */
void socket_forwarding_cmd (int source, int destination, char *cmd) {
    int n, i;
    const size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];

    // pipe_in : father --> child
    // pipe_out: child  --> father
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) { // create command input and output pipes
        log(ERROR, "socket_forwarding_cmd: Cannot create pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if( pid == 0) {
        dup2(pipe_in[READ], STDIN_FILENO); // stdin --> pipe_in[READ]
        dup2(pipe_out[WRITE], STDOUT_FILENO); // stdout --> pipe_out[WRITE]
        close(pipe_in[WRITE]);
        close(pipe_out[READ]);
        log(INFO, "socket_forwarding_cmd: executing command");
        exit( system(cmd) );
    } else {
        close(pipe_in[READ]); // no need to read from input pipe here // close
        close(pipe_out[WRITE]);

        while ((n = recv(source, buffer, BUFFER_SIZE, 0)) > 0) { // read source socket --> write to buffer
            if (write(pipe_in[WRITE], buffer, n) < 0) {
                log(ERROR, "socket_forwarding_cmd: Cannot write to input pipe of external command");
                exit(EXIT_FAILURE);
            }
            else {
                log(INFO, "socket_forwarding_cmd: writing to input pipe of external command");
            }
            if ((i = read(pipe_out[READ], buffer, BUFFER_SIZE)) > 0) {
                log(INFO, "socket_forwarding_cmd: sending command output to destination socket...")
                send(destination, buffer, i, 0);
            }
        }
    }
}