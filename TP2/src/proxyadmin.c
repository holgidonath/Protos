
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/sctp.h>
#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h> // para toupper

#include "include/buffer.h"
#include "include/args.h"
#include "include/stm.h"
#include "include/logger.h"
#include "include/main.h"
#include "include/util.h"
#include "include/metrics.h"
#include "include/selector.h"

#define N(x)                (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key)     ( ( struct admin * )(key)->data)

typedef struct admin
{
    int client_fd;
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;

    buffer read_buffer, write_buffer;
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    struct state_machine stm;
    struct admin * next;
    unsigned                references;
//
//    time_t last_use;
//    admin_state state;

} admin;

typedef enum command_parser_states
{
    BEGIN,
    L,
    LO,
    LOG,
    LOGI,
    LOGIN,
    S,
    ST,
    STA,
    STAT,
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

typedef enum admin_states
{
   GREETING,
   AUTH,
   COMMANDS,
   ADONE,
   AERROR,

} admin_states;

static unsigned greet(struct selector_key *key);
static unsigned recieve_from_client(struct selector_key * key);
//static void hop(const unsigned state, struct selector_key *key);

static unsigned authenticate(struct selector_key * key);
static unsigned send_to_client(struct selector_key * key);

static unsigned parse_command(struct selector_key * key);
static unsigned command_response(struct selector_key * key);

static const struct state_definition client_statbl[] =
        {
        {
                .state          = GREETING,
                .on_write_ready = greet,

        },
        {
                .state          = AUTH,
                .on_read_ready  = authenticate,

        },
        {
                .state          = COMMANDS,
                .on_read_ready  = parse_command,
                .on_write_ready = command_response,
        },
        {
                .state          = ADONE,

        },
        {
                .state          = AERROR,

        }
};

static const struct state_definition *
admin_describe_states(void)
{
    return client_statbl;
};


struct admin *
new_admin(int client_fd);


static void admin_read   (struct selector_key *key);
static void admin_write  (struct selector_key *key);
static void admin_block  (struct selector_key *key);
static void admin_close  (struct selector_key *key);
static void admin_done  (struct selector_key *key);
static void admin_destroy(struct admin * admin);

static const struct fd_handler admin_handler = {
        .handle_read   = admin_read,
        .handle_write  = admin_write,
        .handle_close  = admin_close,
        .handle_block  = admin_block,
};

static void admin_read(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum admin_states st = stm_handler_read(stm,key);

    if (AERROR == st || ADONE == st)
    {
        admin_done(key);
    }
}

static void admin_write(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum admin_states st = stm_handler_write(stm,key);

    if (AERROR == st || ADONE == st)
    {
        admin_done(key);
    }
}


static void admin_block(struct selector_key *key)
{
  //NOTHING TO DO HERE
}

static void
admin_destroy(struct admin * admin){
    if(admin != NULL){
        // struct connection * aux = connections;
        // while(aux->next != NULL && aux->next != con){
        //     aux = aux->next;
        // }
        // aux->next = con->next;
        // if(con->origin_resolution != NULL){
        //     free(con->origin_resolution);
        // }
        // if(con->origin_resolution_current != NULL){
        //     free(con->origin_resolution_current);
        // }
        if(admin->references == 1){
            free(admin);
        } else {
            admin->references -= 1;
        }

    }
}

static void
admin_close(struct selector_key *key) {

    admin_destroy(ATTACHMENT(key));

}


static void

admin_done(struct selector_key* key) {

    const int fds[] = {

            ATTACHMENT(key)->client_fd,

    };

    for(unsigned i = 0; i < N(fds); i++) {

        if(fds[i] != -1) {

            if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {

                abort();

            }

            close(fds[i]);

        }

    }
}
// FALTA IMPLEMENTAR DESTROYERS


void
admin_connection(struct selector_key *key)
{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    struct admin *admin = new_admin(client);
    if(admin == NULL) {
        goto fail;
    }


    memcpy(&admin->client_addr, &client_addr, client_addr_len);
    admin->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &admin_handler, OP_WRITE, admin)) {
        goto fail;
    }


    return;

    fail:
    if(client != -1) {
        close(client);
    }
}

struct admin *
new_admin(int client_fd)
{
    struct admin * admin;
    admin = malloc(sizeof(*admin));

    if (admin != NULL)
    {
        memset(admin, 0x00, sizeof(*admin));

        admin->next = 0;
        admin->client_fd = client_fd;

        admin->stm    .initial = GREETING;
        admin->stm    .max_state = AERROR;
        admin->stm    .states = admin_describe_states();

        admin->references = 1;
        stm_init(&admin->stm);

        buffer_init(&admin->read_buffer, N(admin->raw_buff_a), admin->raw_buff_a);
        buffer_init(&admin->write_buffer, N(admin->raw_buff_b), admin->raw_buff_b);
    }

    return admin;
}


static unsigned greeting(struct selector_key* key){

    admin * admin = ATTACHMENT(key);
    size_t greeting_size;
    buffer * buff = &admin->write_buffer;
    size_t buff_size;

    char * greeting = "Welcome admin!\nLogin: ";
    greeting_size = strlen(greeting);

    uint8_t  * ptr = buffer_write_ptr(buff, &buff_size);
    memcpy(ptr, greeting, greeting_size);
    buffer_write_adv(buff, greeting_size);
    int n = send_to_client(key);
    
    return n;
}

static unsigned incorrect_pass(struct selector_key* key){

    admin * admin = ATTACHMENT(key);
    size_t auth_error_size;
    buffer * buff = &admin->write_buffer;
    size_t buff_size;

    char * auth_error = "Incorrect Password!\nTry again: ";
    auth_error_size = strlen(auth_error);

    uint8_t  * ptr = buffer_write_ptr(buff, &buff_size);
    memcpy(ptr, auth_error, auth_error_size);
    buffer_write_adv(buff, auth_error_size);
    int n = send_to_client(key);


//    selector_set_interest(key->s, admin->client_fd, OP_READ);
    return n;
}

static unsigned greet(struct selector_key* key)
{
    int status = greeting(key);
    if (status < 0)
    {
        return AERROR;
    }
    return AUTH;

}

//static void
//hop(const unsigned state, struct selector_key* key)
//{
//    log(INFO, "successful hopping");
//}

static unsigned
authenticate(struct selector_key * key)
{
    size_t size;
    buffer * buff = &ATTACHMENT(key)->read_buffer;
    char * login_key = "password";
    int bytes = recieve_from_client(key);
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
    uint8_t * ptr = buffer_read_ptr(buff,&size);
    if(strncmp(login_key, ptr, strlen(login_key)) == 0)
    {
        log(INFO, "Correct key");
        buffer_read_adv(buff,bytes);
        return COMMANDS;
    }
    else
    {
        log(ERROR,"Incorrect key");
        buffer_read_adv(buff,bytes);
        incorrect_pass(key);
        return AUTH;
    }

}

static unsigned parse_command(struct selector_key * key)
{
    admin * admin = ATTACHMENT(key);
    buffer * buff = &admin->read_buffer;
    int bytes = recieve_from_client(key);
    size_t size;


    uint8_t * ptr = buffer_read_ptr(buff, &size);
    char c = toupper(*ptr);
    command_parser_states state = BEGIN;
    command_parser_states command;
    char args[50];
    int args_index = 0;
    while(state != INVALID && buffer_can_read(buff))
    {
        switch(state)
        {
            case BEGIN:
                if(c == 'L')
                {
                    state = L;
                }
                else if(c == 'S')
                {
                    state = S;
                }
                else if(c == 'G')
                {
                    state = G;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case L:
                if(c == 'O')
                {
                    state = LO;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LO:
                if(c == 'G')
                {
                    state = LOG;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOG:
                if(c == 'I')
                {
                    state = LOGI;
                }
                else if(c == 'O')
                {
                    state = LOGO;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOGI:
                if(c == 'N')
                {
                    state = LOGIN;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOGIN:
                if(c == ' ')
                {
                    state = ARGUMENTS;
                    command = LOGIN;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOGO:
                if(c == 'U')
                {
                    state = LOGOU;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOGOU:
                if(c == 'T')
                {
                    state = LOGOUT;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case LOGOUT:
                if(c == '\n') //TODO: ver si tengo que hacer para \r\n tambien o solo \n
                {
                    state = CDONE;
                    command = LOGOUT;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case S:
                if(c == 'T')
                {
                    state = ST;
                }
                else if(c == 'E')
                {
                    state = SE;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case SE:
                if(c == 'T')
                {
                    state = SET;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case SET:
                if(c == 'C')
                {
                    state = SETC;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case SETC:
                if(c == 'M')
                {
                    state = SETCM;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case SETCM:
                if(c == 'D')
                {
                    state = SETCMD;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case SETCMD:
                if(c == ' ')
                {
                    state = ARGUMENTS;
                    command = SETCMD;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case ST:
                if(c == 'A')
                {
                    state = STA;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case STA:
                if(c == 'T')
                {
                    state = STAT;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case STAT:
                if(c == ' ')
                {
                    state = ARGUMENTS;
                    command = STAT;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case G:
                if(c == 'E')
                {
                    state = GE;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case GE:
                if(c == 'T')
                {
                    state = GET;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case GET:
                if(c == 'C')
                {
                    state = GETC;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case GETC:
                if(c == 'M')
                {
                    state = GETCM;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case GETCM:
                if(c == 'D')
                {
                    state = GETCMD;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case GETCMD:
                if(c == '\n')//TODO: ver lo de \r\n que puse arriba
                {
                    state = CDONE;
                    command = GETCMD;
                }
                else
                {
                    state = INVALID;
                }
                break;
            case ARGUMENTS:
                if(c != '\n')
                {
                    args[args_index++] = *ptr;
                }
                else
                {
                    state = CDONE;
                }
                break;
            case INVALID:
                buff = &admin->write_buffer;
                ptr = buffer_write_ptr(buff, &size);
                char * invalid = "Invalid command\n";
                memcpy(ptr, invalid, strlen(invalid));
                send_to_client(key);
            case CDONE:
                break;
        }
        buffer_read_adv(buff,1);
        ptr++;
        c = *ptr;
    }

    return COMMANDS;

}

static unsigned command_response(struct selector_key * key)
{

}


static unsigned send_to_client(struct selector_key * key) {

    admin * admin = ATTACHMENT(key);
    buffer * buff = &admin->write_buffer;
    size_t size;
    int n;
    uint8_t * ptr = buffer_read_ptr(buff, &size);
     if(n = sctp_sendmsg(key->fd, ptr , size,
                               NULL, 0, 0, 0, 0, 0, 0) < 0){
         log(ERROR, "Error sending message to client");
         return AERROR; //Nose si deberiamos ir a error
     }
    buffer_read_adv(buff, size);
    selector_set_interest(key->s, admin->client_fd, OP_READ);
    return n;
}

static unsigned recieve_from_client(struct selector_key * key)
{
    admin * admin = ATTACHMENT(key);
    buffer * buff = &admin->read_buffer;
    size_t size;
    int n;
    uint8_t * ptr = buffer_write_ptr(buff, &size);
    n = sctp_recvmsg(key->fd, ptr, size, NULL, 0, 0, 0);
    if(n<= 0)
    {
        log(ERROR, "Admin Connection Terminated");
        shutdown(admin->client_fd,SHUT_RD);
    }
    buffer_write_adv(buff, n);
    selector_set_interest(key->s, admin->client_fd, OP_WRITE);
    return n;

}

