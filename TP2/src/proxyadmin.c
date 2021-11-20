
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

typedef enum admin_states
{
   GREETING,
   HOP,
   ADONE,
   AERROR,

} admin_states;

static unsigned
readfromclient(struct selector_key* key);

static unsigned greet(struct selector_key *key);

static void hop(const unsigned state, struct selector_key *key);


static const struct state_definition client_statbl[] =
        {
        {
                .state          = GREETING,
                .on_write_ready = greet,

        },
        {
                .state          = HOP,
                .on_arrival     = hop,

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
    size_t size;

    char * hello = "Bienvenido";
    size = strlen(hello);


    ssize_t ret = sctp_sendmsg(key->fd, hello , size,
                               NULL, 0, 0, 0, 0, 0, 0);

    selector_set_interest(key->s, admin->client_fd, OP_NOOP);
    return ret;
}

static unsigned greet(struct selector_key* key)
{
    int status = greeting(key);
    if (status < 0)
    {
        return AERROR;
    }
    return HOP;

}

static void
hop(const unsigned state, struct selector_key* key)
{
    log(INFO, "successful hopping");
}

