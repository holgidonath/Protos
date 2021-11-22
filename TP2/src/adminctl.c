#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/sctp.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "include/argsctl.h"

#define STATSC 0
#define GETCMDC 1
#define SETCMDC 2
#define LOGOUTC 3
#define HELPC    4

typedef enum command_parser_states
{
    BEGIN = 20,
    L,
    LO,
    LOG,
    LOGI,
    LOGIN,
    H,
    HE,
    HEL,
    HELP,
    S,
    ST,
    STA,
    STAT,
    STATS,
    G,
    GE,
    GET,
    GETC,
    GETCM,
    GETCMD,
    SE,
    SET,
    SETC,
    SETCM,
    SETCMD,
    LOGO,
    LOGOU,
    LOGOUT,
    ARGUMENTS,
    INVALID,
    CDONE,


} command_parser_states;

char * help_message = "These are all available commmands:\n"
                      "STATS\t\tPrints useful statistics about the proxy server\n"
                      "GETCMD\t\tPrints the current filter command\n"
                      "SETCMD <cmd>\tSets new filter command\n"
                      "LOGOUT\t\tDisconnects admin client\n"
                      "HELP\t\tPrints this message\n";

static unsigned parse_command(int sock, char * in_buff, char * out_buffer) {
    memset(out_buffer, 0, BUFF_SIZE);
    memset(in_buff, 0, BUFF_SIZE);
    fgets(out_buffer, BUFF_SIZE, stdin);
//    strtok(out_buffer, "\n");
    if (strlen(out_buffer) < 0)
    {
        return -1;
    }
    char c = toupper(out_buffer[0]);
    command_parser_states state = BEGIN;
    int command = -1;
    char args[100] = {0};
    int args_index = 0;
    int i = 0;
    while (state != INVALID && out_buffer[i] != 0) {
        switch (state) {
            case BEGIN:
                if (c == 'L') {
                    state = L;
                } else if (c == 'S') {
                    state = S;
                } else if (c == 'G') {
                    state = G;
                } else if (c == 'H'){
                    state = H;
                }
                else {
                    state = INVALID;
                }
                break;
            case L:
                if (c == 'O') {
                    state = LO;
                } else {
                    state = INVALID;
                }
                break;
            case LO:
                if (c == 'G') {
                    state = LOG;
                } else {
                    state = INVALID;
                }
                break;
            case LOG:
                if (c == 'O') {
                    state = LOGO;
                } else {
                    state = INVALID;
                }
                break;
            case LOGO:
                if (c == 'U') {
                    state = LOGOU;
                } else {
                    state = INVALID;
                }
                break;
            case LOGOU:
                if (c == 'T') {
                    state = LOGOUT;
                } else {
                    state = INVALID;
                }
                break;
            case LOGOUT:
                if (c == '\n') //TODO: ver si tengo que hacer para \r\n tambien o solo \n
                {
                    state = CDONE;
                    command = LOGOUTC;
                } else {
                    state = INVALID;
                }
                break;
            case S:
                if (c == 'T') {
                    state = ST;
                } else if (c == 'E') {
                    state = SE;
                } else {
                    state = INVALID;
                }
                break;
            case SE:
                if (c == 'T') {
                    state = SET;
                } else {
                    state = INVALID;
                }
                break;
            case SET:
                if (c == 'C') {
                    state = SETC;
                } else {
                    state = INVALID;
                }
                break;
            case SETC:
                if (c == 'M') {
                    state = SETCM;
                } else {
                    state = INVALID;
                }
                break;
            case SETCM:
                if (c == 'D') {
                    state = SETCMD;
                } else {
                    state = INVALID;
                }
                break;
            case SETCMD:
                if (c == ' ') {
                    state = ARGUMENTS;
                    command = SETCMDC;
                } else {
                    state = INVALID;
                }
                break;
            case ST:
                if (c == 'A') {
                    state = STA;
                } else {
                    state = INVALID;
                }
                break;
            case STA:
                if (c == 'T') {
                    state = STAT;
                } else {
                    state = INVALID;
                }
                break;
            case STAT:
                if (c == 'S') {
                    state = STATS;
                } else {
                    state = INVALID;
                }
                break;
            case STATS:
                if (c == '\n') {
                    state = CDONE;
                    command = STATSC;
                } else {
                    state = INVALID;
                }
                break;
            case G:
                if (c == 'E') {
                    state = GE;
                } else {
                    state = INVALID;
                }
                break;
            case GE:
                if (c == 'T') {
                    state = GET;
                } else {
                    state = INVALID;
                }
                break;
            case GET:
                if (c == 'C') {
                    state = GETC;
                } else {
                    state = INVALID;
                }
                break;
            case GETC:
                if (c == 'M') {
                    state = GETCM;
                } else {
                    state = INVALID;
                }
                break;
            case GETCM:
                if (c == 'D') {
                    state = GETCMD;
                } else {
                    state = INVALID;
                }
                break;
            case GETCMD:
                if (c == '\n')//TODO: ver lo de \r\n que puse arriba
                {
                    state = CDONE;
                    command = GETCMDC;
                } else {
                    state = INVALID;
                }
                break;
            case H:
                if(c == 'E'){
                    state = HE;
                } else {
                    state = INVALID;
                }
                break;
            case HE:
                if(c == 'L'){
                    state = HEL;
                } else {
                    state = INVALID;
                }
                break;
            case HEL:
                if(c == 'P'){
                    state = HELP;
                } else {
                    state = INVALID;
                }
                break;
            case HELP:
                if(c == '\n'){
                    state = CDONE;
                    command = HELPC;
                } else {
                    state = INVALID;
                }
                break;
            case ARGUMENTS:
                if (c != '\n') {
                    args[args_index++] = out_buffer[i];
                } else {
                    state = CDONE;
                }
                break;

            case CDONE:
                break;
        }
        c = toupper(out_buffer[++i]);
    }

    int n;
    if (state == INVALID)
    {
        printf("Invalid command!\n");
        return -1;
    } else {
        memset(out_buffer, 0, BUFF_SIZE);
        memset(in_buff, 0, BUFF_SIZE);
        sprintf(out_buffer, "%d", command);
        if(strlen(args) > 0){
            strcat(out_buffer, args);
        }
        n = sctp_sendmsg(sock, out_buffer, strlen(out_buffer), NULL, 0,0,0,0,0,0);
        if(n < 0){
            printf("Error sending command!\n");
            return -1;
        }
        n = sctp_recvmsg(sock,in_buff, BUFF_SIZE, NULL, 0,0,0);
        if(n < 0){
            printf("Error receiving response!\n");
            return -1;
        }

        switch (command) {
            case GETCMDC:
                if(in_buff[0] == '+')
                {
                    printf(in_buff+1);
                }
                else if (in_buff[0] == '-')
                {
                    printf("No command is set\n");
                }
                else
                {
                    printf("Could not get command\n");
                }
                break;
            case STATSC:
                if(in_buff[0] == '+')
                {
                    printf(in_buff+1);
                }
                else if (in_buff[0] == '-')
                {
                    printf("Failed to get stats\n");
                }
                break;
            case SETCMDC:
                if(in_buff[0] == '+')
                {
                    printf("New filter command set to %s\n", args);
                }
                else if (in_buff[0] == '-')
                {
                    printf("Failed to set new filter command\n");
                }
                break;
            case LOGOUTC:
                if(in_buff[0] == '+')
                {
                    printf("Logging out...\n");
                }
                else if (in_buff[0] == '-')
                {
                    printf("Failed to log out\n");
                }
                break;
            case HELPC:
                printf(help_message);
                break;
        }


    }

    return 1;

}

int get_authentication(int sock,char * in_buffer,char * out_buffer)
{
    int n;
    int nr;
    memset(in_buffer, 0, BUFF_SIZE);
    memset(out_buffer, 0, BUFF_SIZE);
    fgets(out_buffer, BUFF_SIZE, stdin);
    strtok(out_buffer, "\n");
    if (n = sctp_sendmsg(sock, out_buffer, strlen(out_buffer),
                         NULL, 0, 0, 0, 0, 0, 0) < 0) {
        printf("ERROR\n");
        return -1;
    }
    nr = sctp_recvmsg(sock, in_buffer, BUFF_SIZE, NULL, 0, 0, 0);
    if (nr <= 0) {
        printf("ERROR\n");
        return -1;
    }
    if (in_buffer[0] == '+')
    {
        //printf(in_buffer);
        printf("Welcome Administrator!\n");
        return  1;
    }
    else
    {
        //printf(in_buffer);
        printf("Wrong Password, Try Again\n");
        return 0;
    }

}


int main(const int argc, char **argv) {

    int status = 0;
    struct admin_opt opt;
    struct address_data addr;
    char out[BUFF_SIZE] = {0};
    char incoming[BUFF_SIZE] = {0};
    parse_admin_options(argc, argv, &opt);
    set_mgmt_address(&addr, opt.mgmt_addr, &opt);
    int sock = socket(addr.mgmt_domain, SOCK_STREAM, IPPROTO_SCTP);
    if(sock < 0)
    {
        printf("Failed to create socket\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    int con = connect(sock, (struct sockaddr*)&addr.mgmt_addr, addr.mgmt_addr_len);
    if (con < 0)
    {
        printf("Failed to connect to management\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    int n = sctp_recvmsg(sock, incoming, BUFF_SIZE, NULL, 0,0,0);
    printf(incoming);
    if(n < 0)
    {
        printf("Error getting greeting\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Please enter Password to enter.\nPassword: \n");
    while(!status)
    {
        status = get_authentication(sock,incoming, out);

    }
    printf(help_message);
    while(1){
        parse_command(sock, incoming, out);
    }
    return 0;
}
