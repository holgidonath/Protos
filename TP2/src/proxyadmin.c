
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
#include "include/proxytcp.h"


#define N(x)                (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key)     ( ( struct admin * )(key)->data)

typedef enum command_parser_states
{

    STATS = 0,
    GETCMD,
    SETCMD,
    LOGOUT,


} command_parser_states;

typedef enum admin_states
{
    GREETING,
    AUTH,
    COMMANDS,
    ADONE,
    AERROR,

} admin_states;
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
    admin_states state;



} admin;



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
        if(admin->references == 1)

            free(admin);
        } else {
            admin->references -= 1;
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
        admin->state = GREETING;

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

    char * greeting = "+Greetings\n";
    greeting_size = strlen(greeting);

    uint8_t  * ptr = buffer_write_ptr(buff, &buff_size);
    memcpy(ptr, greeting, greeting_size);
    buffer_write_adv(buff, greeting_size);
    int n = send_to_client(key);
    
    return n;
}

static unsigned pass_auth(struct selector_key* key, bool number){

    admin * admin = ATTACHMENT(key);
    size_t auth_error_size;
    buffer * buff = &admin->write_buffer;
    size_t buff_size;
    int n;
    if (number == 0)
    {
        char *auth_error = "-ERR\n";
        auth_error_size = strlen(auth_error);
        uint8_t *ptr = buffer_write_ptr(buff, &buff_size);
        memcpy(ptr, auth_error, auth_error_size);
        buffer_write_adv(buff, auth_error_size);
        n = send_to_client(key);
    }
    else
    {
        char *auth_error = "+OK\n";
        auth_error_size = strlen(auth_error);
        uint8_t *ptr = buffer_write_ptr(buff, &buff_size);
        memcpy(ptr, auth_error, auth_error_size);
        buffer_write_adv(buff, auth_error_size);
        n = send_to_client(key);
    }

    return n;
}

static unsigned greet(struct selector_key* key)
{
    int status = greeting(key);
    if (status < 0)
    {
        ATTACHMENT(key)->state = AERROR;
        return AERROR;
    }
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
    ATTACHMENT(key)->state = AUTH;
    return AUTH;

}

static unsigned
authenticate(struct selector_key * key)
{
    size_t size;
    int status;
    buffer * buff = &ATTACHMENT(key)->read_buffer;
    char * login_key = "password";
    int bytes = recieve_from_client(key);
    if(bytes < 0)
    {
        return AERROR;
    }
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
    uint8_t * ptr = buffer_read_ptr(buff,&size);
    if(strncmp(login_key, ptr, strlen(login_key)) == 0)
    {
        status = pass_auth(key,1);
        if (status < 0)
        {
            return AERROR;
        }
        buffer_read_adv(buff,bytes);
        ATTACHMENT(key)->state = COMMANDS;
        return COMMANDS;
    }
    else
    {
        status = pass_auth(key,0);
        if (status < 0)
        {
            return AERROR;
        }
        buffer_read_adv(buff,bytes);
        return AUTH;
    }

}



static unsigned command_response(struct selector_key * key)
{
    int status;
    admin * admin = ATTACHMENT(key);
    status = send_to_client(key);
    if(status < 0)
    {
        return AERROR;
    }
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
    return admin->state;
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
         return -1;
     }
    buffer_read_adv(buff, size);
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
     return -1;
    }
    buffer_write_adv(buff, n);
    return n;

}

static unsigned parse_command(struct selector_key * key) {

    char buf[2048] = {0};
    admin *admin = ATTACHMENT(key);
    buffer *buff = &admin->read_buffer;
    int bytes = recieve_from_client(key);
    if (bytes < 0)
    {
        return AERROR;
    }
    size_t size;
    uint8_t *ptr = buffer_read_ptr(buff, &size);
    int comando = *ptr - '0';
    buffer_read_adv(buff, 1);
    buff = &admin->write_buffer;
    ptr = buffer_write_ptr(buff, &size);
    char *message;
    struct opt * opt;
    selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
    int flag = 0;

    switch (comando)
    {

        case STATS:

            message = get_stats();
            if(message == NULL)
            {
                message = "-ERR\n";
                memcpy(ptr, message, strlen(message));
            }
            else
            {
                memcpy(ptr, message, strlen(message));
            }
            buffer_write_adv(buff, strlen(message));
            admin->state = COMMANDS;
            free(message);
            break;

        case GETCMD:

            opt = get_opt();
            if(opt->cmd != NULL)
            {
                message = malloc(2048);
                memset(message,0, 2048);
                message[0] = '+';
                strcat(message, opt->cmd);
                strcat(message, "\n");
                flag = 1;

            } else {
                message = "-ERR\n";
            }
            memcpy(ptr, message, strlen(message));
            buffer_write_adv(buff, strlen(message));
            if(flag){
                free(message);
            }
            admin->state = COMMANDS;
            break;

        case SETCMD:

            opt = get_opt();
            if (opt == NULL)
            {
                message = "-ERR\n";
            }
            else {
                buff = &admin->read_buffer;
                ptr = buffer_read_ptr(buff,&size);
                int j = 0;
                while(buffer_can_read(buff)){
                    buf[j] = ptr[j];
                    j++;
                    buffer_read_adv(buff, 1);
                }
                buff = &admin->write_buffer;
                ptr = buffer_read_ptr(buff,&size);
                if(opt->cmd == NULL)
                {
                    opt->cmd = malloc(sizeof(buf));
                }
                strcpy(opt->cmd, buf);
                message = "+OK\n";
            }
            memcpy(ptr, message, strlen(message));
            buffer_write_adv(buff, strlen(message));
            admin->state = COMMANDS;
            log(INFO, "Command changed to %s", opt->cmd);
            break;

        case LOGOUT:

            message = "+OK\n";
            memcpy(ptr, message, strlen(message));
            buffer_write_adv(buff, strlen(message));
            admin->state = ADONE;
            break;

    }

    return COMMANDS;

}

