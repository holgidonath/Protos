/**

 */

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
#include "include/proxyadmin.h"
//#include "include/socket_admin.h"
#include "include/extcmd.h"
#include "include/proxytcp.h"

#define N(x)                (sizeof(x)/sizeof((x)[0]))
#define ATTACHMENT(key)     ( ( struct connection * )(key)->data)
#define READ                0
#define WRITE               1
#define BEGIN               20
#define R                   21
#define RE                  22          //a partir del 20 para poder usar tranquilamente constantes que necesitemos 
#define RET                 23
#define RETR                24
#define U                   25
#define US                  26
#define USE                 27
#define USER                28
#define C                   29
#define CA                  30
#define CAP                 31
#define CAPA                32
#define DONEUSER            33
#define DONERETR            34
#define FORWARD             35


typedef enum address_type {
    ADDR_IPV4   = 0x01,
    ADDR_IPV6   = 0x02,
    ADDR_DOMAIN = 0x03,
} address_type;

typedef union address {
    char                    fqdn[0xFF];
    struct sockaddr_storage addr_storage;
} address;

typedef struct address_data {
   
    in_port_t origin_port;
    address origin_addr;
    address_type origin_type;
    socklen_t origin_addr_len;
    int origin_domain;


} address_data;

struct opt opt;
const char *appname;

enum proxy_states
{
    RESOLVE_ORIGIN,
    CONNECT, 
    GREETING,
    EXTERN_CMD,
    COPY,
    DONE,
    PERROR
};

struct copy
{
    int *fd;
    buffer *rb, *wb;
    fd_interest duplex;
    struct copy *other;
 
};

static metrics_t metrics;
// typedef struct client
// {

//     int client_fd;
//     struct copy copy_client;


// } client;

// typedef struct origin 
// {
//     int origin_fd;
//     uint16_t origin_port;
//     address origin_addr;
//     address_type origin_type;
//     int origin_domain;
//     socklen_t origin_addr_len;
//     struct addrinfo *origin_resolution;
//     struct addrinfo *origin_resolution_current;
//     struct copy copy;



// } origin;

struct connection 
{
    int client_fd;
    struct copy copy_client;
    int origin_fd;
    struct address_data origin_data;
    struct addrinfo *origin_resolution;
    struct addrinfo *origin_resolution_current;
    struct copy copy_origin;
    buffer read_buffer, write_buffer;
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    struct state_machine stm;
    struct connection * next;
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len;
    unsigned                references;
    bool                    was_greeted;

    struct extern_cmd       extern_cmd;
    uint8_t                 raw_extern_read_buffer[2048];
    buffer                  extern_read_buffer;
    int                     extern_read_fd;
    int                     extern_write_fd;
};

static struct connection * connections = NULL;

static unsigned
origin_connect(struct selector_key * key);


static void
copy_init(const unsigned state, struct selector_key *key);

static unsigned
copy_r(struct selector_key *key);

static unsigned
copy_w(struct selector_key *key);

static unsigned
connection_ready(struct selector_key  *key);

static unsigned
resolve_done(struct selector_key * key);

static void *
resolve_blocking(void * data);

static void
extern_cmd_init(const unsigned state, struct selector_key *key);

static void
extern_cmd_close(const unsigned state, struct selector_key *key);

static unsigned
extern_cmd_read(struct selector_key *key);

static unsigned
extern_cmd_write(struct selector_key *key);

static enum extern_cmd_status
socket_forwarding_cmd(struct selector_key *key, char * cmd);

void parse_command(char * command);
void parse_response(char * command);
bool parse_greeting(char * command, struct selector_key *key);
char * parse_user(char * ptr);

static const struct state_definition client_statbl[] = 
{
    {
        .state            = RESOLVE_ORIGIN,
        .on_block_ready   = resolve_done,
    },
    {
        .state = CONNECT,
        .on_write_ready = connection_ready,

    },
    {
        .state = GREETING,
    },
    {
        .state = COPY,
        .on_arrival = copy_init,
        .on_read_ready = copy_r,
        .on_write_ready = copy_w,
    },
    {
        .state            = EXTERN_CMD,
        .on_arrival       = extern_cmd_init,
        .on_read_ready    = extern_cmd_read,
        .on_write_ready   = extern_cmd_write,
        .on_departure     = extern_cmd_close,
    },
    {
        .state = DONE,
    },
    {
        .state = PERROR,
    },

};


static const struct state_definition *
proxy_describe_states(void)
{
   return client_statbl;
};

struct opt * get_opt(void){
    return &opt;
}

void 
set_origin_address(struct address_data * address_data, const char * adress) 
{
    
    memset(&(address_data->origin_addr.addr_storage), 0, sizeof(address_data->origin_addr.addr_storage));
    
    address_data->origin_type = ADDR_IPV4;
    address_data->origin_domain  = AF_INET;
    address_data->origin_addr_len = sizeof(struct sockaddr_in);
    

    struct sockaddr_in ipv4; 
    memset(&(ipv4), 0, sizeof(ipv4));
    ipv4.sin_family = AF_INET;
    int result = 0;

    if((result = inet_pton(AF_INET, adress, &ipv4.sin_addr.s_addr)) <= 0) 
    {
        address_data->origin_type   = ADDR_IPV6;
        address_data->origin_domain  = AF_INET6;
        address_data->origin_addr_len = sizeof(struct sockaddr_in6);
    

        struct sockaddr_in6 ipv6; 

        memset(&(ipv6), 0, sizeof(ipv6));

        ipv6.sin6_family = AF_INET6;

        if((result = inet_pton(AF_INET6, adress, &ipv6.sin6_addr.s6_addr)) <= 0)
        {
            memset(&(address_data->origin_addr.addr_storage), 0, sizeof(address_data->origin_addr.addr_storage));
            address_data->origin_type   = ADDR_DOMAIN;
            memcpy(address_data->origin_addr.fqdn, adress, strlen(adress));
            address_data->origin_port = opt.origin_port;
            return;
        }

        ipv6.sin6_port = htons(opt.origin_port); 
        memcpy(&address_data->origin_addr.addr_storage, &ipv6, address_data->origin_addr_len);    
        return;
    }    
    ipv4.sin_port = htons(opt.origin_port); 
    memcpy(&address_data->origin_addr.addr_storage, &ipv4, address_data->origin_addr_len);
    return;
}

//---------------------------------------------------------------------------------------



//       HANDLERS QUE EMITEN LOS EVENTOS DE LA MAQUINA DE ESTADOS


//------------------------------------------------------------------------------------------

static void proxy_read   (struct selector_key *key);
static void proxy_write  (struct selector_key *key);
static void proxy_block  (struct selector_key *key);
static void proxy_close  (struct selector_key *key);
static void proxy_done  (struct selector_key *key);
static void proxy_destroy(struct connection * con);
static const struct fd_handler proxy_handler = {
    .handle_read   = proxy_read,
    .handle_write  = proxy_write,
    .handle_close  = proxy_close,
    .handle_block  = proxy_block,
};

static void proxy_read(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum proxy_states st = stm_handler_read(stm,key);

    if (PERROR == st || DONE == st)
    {
       proxy_done(key);
    }
}

static void proxy_write(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum proxy_states st = stm_handler_write(stm,key);

    if (PERROR == st || DONE == st)
    {
       proxy_done(key);
    }
}


static void proxy_block(struct selector_key *key)
{
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum proxy_states st = stm_handler_block(stm,key);

    if (PERROR == st || DONE == st)
    {
        proxy_done(key);
    }
}

static void
proxy_destroy(struct connection * con){
    if(con != NULL){
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
        if(con->references == 1){
            free(con);
            metrics->concurrent_connections--;
        } else {
            con->references -= 1;
        }
        
    }
}

static void
proxy_close(struct selector_key *key) {

   proxy_destroy(ATTACHMENT(key));

}


static void

proxy_done(struct selector_key* key) {

  const int fds[] = {

    ATTACHMENT(key)->client_fd,

    ATTACHMENT(key)->origin_fd,

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




//---------------------------------------------------------------------------------------



//                              CONNECTIONS


//------------------------------------------------------------------------------------------



struct connection * 
new_connection(int client_fd)
{
    struct connection * con;
    con = malloc(sizeof(*con));

    if (con != NULL)
    {
        memset(con, 0x00, sizeof(*con));
        
        con->origin_fd = -1;
        con->client_fd = client_fd;

        con->stm    .initial = RESOLVE_ORIGIN;
        con->stm    .max_state = PERROR;
        con->stm    .states = proxy_describe_states();

        con->references = 1;
        con->was_greeted = false;
        stm_init(&con->stm);

        buffer_init(&con->read_buffer, N(con->raw_buff_a), con->raw_buff_a);
        buffer_init(&con->write_buffer, N(con->raw_buff_b), con->raw_buff_b);
    }

    metrics->concurrent_connections++;
    metrics->total_connections++;

    return con;
}

static unsigned connection_ready(struct selector_key  *key){
    struct connection *con = ATTACHMENT(key);
    int error = 0;
    socklen_t len = sizeof(error);
    if (con->origin_data.origin_type == ADDR_DOMAIN) {
        log(INFO, "con->fd = %d         key->fd = %d\n", con->origin_fd, key->fd);
        if (getsockopt(con->origin_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error != 0) {
                log(INFO, "ooooo");
                con->origin_resolution = con->origin_resolution->ai_next;
                selector_set_interest(key->s, key->fd, OP_NOOP);
                return origin_connect(key);
            } else {
                log(INFO, "origin sever connection success.");
                ATTACHMENT(key)->references += 1;
                freeaddrinfo(con->origin_resolution);
                con->origin_resolution = 0;
                return COPY;
            }
        }
    }
    else
    {
        log(INFO, "con->fd = %d         key->fd = %d\n", con->origin_fd, key->fd);
        if (getsockopt(con->origin_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error != 0) {
                log(ERROR, "failed to connect to ORIGIN");
            } else {
                log(INFO, "origin sever connection success.");
                ATTACHMENT(key)->references += 1;
                return COPY;
            }
        }
    }
}

static unsigned origin_connect(struct selector_key * key) {

    struct connection * con = ATTACHMENT(key);
    enum proxy_states stm_next_status = CONNECT;
    con->origin_fd = socket(con->origin_data.origin_domain, SOCK_STREAM, 0);

    if (con->origin_fd < 0) {
        perror("socket() failed");
        goto error;
    }

    if (selector_fd_set_nio(con->origin_fd) == -1) {
        goto error;
    }


    log(INFO, "Attempting to connect to origin");
    if(con->origin_data.origin_type == ADDR_DOMAIN) {
        char *str;
        struct sockaddr_in *addr;

            addr = con->origin_resolution->ai_addr;
            str = inet_ntoa((struct in_addr) addr->sin_addr);
            if (con->origin_resolution == NULL)
            {
                goto error;
            }
            if (connect(con->origin_fd, addr, con->origin_resolution->ai_addrlen) == -1
                    ) {
                if (errno == EINPROGRESS) {
                    selector_status st = selector_set_interest(key->s, con->client_fd, OP_NOOP);
                    if (SELECTOR_SUCCESS != st) {
                        perror("selector_status_failed");
                        goto error;
                    }
                    st = selector_register(key->s, con->origin_fd, &proxy_handler, OP_WRITE, con);
                    if (SELECTOR_SUCCESS != st) {
                        perror("selector_regiser_failed");
                        goto error;
                    }

                }
            }

    }else {
        if (connect(con->origin_fd, (struct sockaddr *)&con->origin_data.origin_addr.addr_storage, con->origin_data.origin_addr_len) == -1
                    ) {
                if (errno == EINPROGRESS) {
                     selector_status st = selector_set_interest(key->s, con->client_fd, OP_NOOP);
                        if (SELECTOR_SUCCESS != st) {
                             perror("selector_status_failed");
                            goto error;
                        }
                      st = selector_register(key->s, con->origin_fd, &proxy_handler, OP_WRITE, con);
                    if (SELECTOR_SUCCESS != st) {
                        perror("selector_regiser_failed");
                        goto error;
                    }
                    ATTACHMENT(key)->references += 1;

                 } 

            }
        else {
                // Caso que conecte de una, CREO que no deberiamos tirar abort porque si entramos aca ya conecto
                // abort();
            }
    }
	

    return stm_next_status;

    error:
    stm_next_status = PERROR;
    log(ERROR, "origin server connection.");
    if (con->origin_fd != -1) {
        close(con->origin_fd);
    }

    return stm_next_status;
}


void
proxy_tcp_connection(struct selector_key *key)
{
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    pthread_t thread_id;
   

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    struct connection *connection = new_connection(client);
    if(connection == NULL) {
        goto fail;
    }


    memcpy(&connection->client_addr, &client_addr, client_addr_len);
    connection->client_addr_len = client_addr_len;

    set_origin_address(&connection->origin_data, opt.origin_server);

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &proxy_handler, OP_READ, connection)) {
        goto fail;
    }


    key->data = connection;
    if (connection->origin_data.origin_type != ADDR_DOMAIN)
    {
        connection->stm.initial = origin_connect(key);

    }

    else
    {

        struct selector_key* new_key = malloc(sizeof(*key));

        new_key->s = key->s;
        new_key->fd = client;
        new_key->data = connection;

        
        if( pthread_create(&thread_id, 0, resolve_blocking, new_key) == -1 )
        {
            log(ERROR, "function resolve_start, pthread_create error.");
        }

    }
    

    return;

fail:
    if(client != -1) {
        close(client);
    }
}


static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}



//---------------------------------------------------------------------------------------



//                          CREATE PASSIVE SOCKETS


//------------------------------------------------------------------------------------------



int
create_proxy_socket(struct sockaddr_in addr, struct opt opt)
{

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(opt.local_port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
       perror("unable to create proxy socket");
        return -1;
    }

    fprintf(stdout, "Listening on TCP port %d\n", opt.local_port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("unable to bind proxy socket");
        return -1;
    }

    if (listen(server, 20) < 0) {
        perror("unable to listen in proxy socket");
        return -1;
    }

    return server;
}

int
create_management_socket(struct sockaddr_in addr, struct opt opt)
{

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(opt.mgmt_port);

    const int admin = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if(admin < 0) {
       perror("unable to create management socket");
        return -1;
    }

    fprintf(stdout, "Listening on SCTP port %d\n", opt.mgmt_port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(admin, IPPROTO_SCTP, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(admin, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("unable to bind management socket");
        return -1;
    }

    if (listen(admin, 20) < 0) {
        perror("unable to listen in management socket");
        return -1;
    }

    return admin;
}


//-----------------------------------------------------------------------------------------------------------------
//                                          RESOLVE_ORIGIN FUNCTIONS
//----------------------------------------------------------------------------------------------------------------
static unsigned
resolve_start(struct selector_key *key) {
    enum proxy_states stm_next_status = ERROR;

    struct selector_key* new_key = safe_malloc(sizeof(*key));
    memcpy(new_key, key, sizeof(*new_key));

    pthread_t thread_id;
    if( pthread_create(&thread_id, 0, resolve_blocking, new_key) != -1 ) {
        stm_next_status = RESOLVE_ORIGIN;
        selector_set_interest_key(key, OP_NOOP);
        pthread_join(thread_id, NULL);
    } else {
        log(ERROR, "function resolve_start, pthread_create error.");
    }

    return stm_next_status;
}

static void *
resolve_blocking(void * data) {
    struct selector_key  *key = (struct selector_key *) data;
    struct connection * connection = ATTACHMENT(key);

    pthread_detach(pthread_self());
    
    connection->origin_resolution = 0;
    struct addrinfo hints = {
        .ai_family    = AF_UNSPEC,    
        /** Permite IPv4 o IPv6. */
        .ai_socktype  = SOCK_STREAM,  
        .ai_flags     = AI_PASSIVE,   
        .ai_protocol  = 0,        
        .ai_canonname = NULL,
        .ai_addr      = NULL,
        .ai_next      = NULL,
    };

    char buff[7];
    snprintf(buff, sizeof(buff), "%d", connection->origin_data.origin_port);

    if( getaddrinfo(connection->origin_data.origin_addr.fqdn,
                    buff,
                    &hints,
                    &connection->origin_resolution
                    ) != 0 ) 
    {
        log(INFO, "connection_resolve_blocking, couldn't resolve address");

    }
    // end of blocking task
    selector_notify_block(key->s, key->fd);

    free(data);
    return 0;
}


static unsigned
resolve_done(struct selector_key * key) 
{
    struct connection * connection = ATTACHMENT(key);
    if(connection->origin_resolution != 0) 
    {
        connection->origin_data.origin_domain = connection->origin_resolution->ai_family;
        connection->origin_data.origin_addr_len = connection->origin_resolution->ai_addrlen;
        memcpy(&connection->origin_data.origin_addr.addr_storage,
                connection->origin_resolution->ai_addr,
                connection->origin_resolution->ai_addrlen);
    } else 
    {

        log(ERROR, "Failed to resolve origin domain\n");
    }

    return origin_connect(key);
}



//-----------------------------------------------------------------------------------------------------------------
//                                          COPY FUNCTIONS
//-----------------------------------------------------------------------------------------------------------------

static void
copy_init(const unsigned state, struct selector_key *key)
{
    struct copy * d = &ATTACHMENT(key)->copy_client;

    d->fd       = &ATTACHMENT(key)->client_fd;
    d->rb       = &ATTACHMENT(key)->read_buffer;
    d->wb       = &ATTACHMENT(key)->write_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->copy_origin;

    d			= &ATTACHMENT(key)->copy_origin;
    d->fd       = &ATTACHMENT(key)->origin_fd;
    d->rb       = &ATTACHMENT(key)->write_buffer;
    d->wb       = &ATTACHMENT(key)->read_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->copy_client;
}

static fd_interest
copy_compute_interests(fd_selector s, struct  copy* d)
{
    fd_interest ret = OP_NOOP;
    if((d->duplex & OP_READ) && buffer_can_write(d->rb))
    {
        ret |= OP_READ;
    }
    if((d->duplex & OP_WRITE) && buffer_can_read(d->wb))
    {
        ret |= OP_WRITE;
    }
    if(SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret))
    {
        abort();
    }
    return ret;
}

static struct copy *
copy_ptr(struct selector_key * key)
{
    struct copy *d = &ATTACHMENT(key)->copy_client;

    if(*d->fd == key->fd)
    {
        //ok
    }
    else
    {
        d = d->other;
    }
    return d;
}

static unsigned
copy_r(struct selector_key *key)
{
    struct copy *d = copy_ptr(key);
    log(INFO, "d->fd = %d         key->fd = %d\n", *d->fd, key->fd);
    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b       = d->rb;
    unsigned ret    = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);

    metrics->bytes_transfered += n;

    if(key->fd == ATTACHMENT(key)->client_fd){
        if(ptr[0] == 'R'){
            log(INFO,"possible retr found");
        }
        parse_command(ptr);
    } 

    // log(INFO,"message captured");

    if(n <=0 )
    {
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~OP_READ;
        if(*d->other->fd != -1)
        {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~OP_WRITE;
        }
    }
    else
    {
        buffer_write_adv(b,n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if(d->duplex == OP_NOOP)
    {
        ret = DONE;
    }
    return ret;
}

static unsigned
copy_w(struct selector_key *key)
{
    struct copy *d = copy_ptr(key);
    log(INFO, "d->fd = %d         key->fd = %d\n", *d->fd, key->fd);
    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b       = d->wb;
    unsigned ret    = COPY;

    uint8_t *ptr = buffer_read_ptr(b, &size);

     if(key->fd == ATTACHMENT(key)->client_fd){
         if(!ATTACHMENT(key)->was_greeted){
             bool greeting = parse_greeting(ptr, key);
             if(greeting){
                log(INFO, "greeting recieved");
             }
         }else {
             // parse_response();
         }
        
     }

    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if(n == -1)
    {
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~OP_WRITE;
        if(*d->other->fd != -1)
        {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~OP_READ;
        }
    }
    else
    {
        buffer_read_adv(b,n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if(d->duplex == OP_NOOP)
    {
        ret = DONE;
    }
    return ret;
}

//-----------------------------------------------------------------------------------------------------------------
//                                          PARSING AND POSTERIOR HANDLING FUNCTIONS
//-----------------------------------------------------------------------------------------------------------------
void parse_command(char * ptr){
    int i = 0;
    int state = BEGIN;
    char c = toupper(ptr[0]);
    while(state != FORWARD){
       switch(state){
           case BEGIN:
           if(c == 'R'){
               state = R;
           }else if(c == 'U'){
               state = U;
           }else if(c == 'C'){
               state = C;
           }else{
               state = FORWARD;
           }
           break;
           case R:
           if(c == 'E'){
               state = RE;
           }else{
               state = FORWARD;
           }
           break;
           case RE:
           if(c == 'T'){
               state = RET;
           }else{
               state = FORWARD;
           }
           break;
           case RET:
           if(c == 'R'){
               state = RETR;
           }else{
               state = FORWARD;
           }
           break;
           case RETR:
           if(c == ' '){
                state = DONERETR;
           } else {
               state = FORWARD;
           }
           break;
           case U:
           if(c == 'S'){
               state = US;
           }else{
               state = FORWARD;
           }
           break;
           case US:
           if(c == 'E'){
               state = USE;
           }else{
               state = FORWARD;
           }
           break;
           case USE:
           if(c == 'R'){
               state = USER;
           }else{
               state = FORWARD;
           }
           break;
           case USER:
           if(c == ' '){
               state = DONEUSER;
           }else{
               state = FORWARD;
           }
           break;
           case C:
           if(c == 'A'){
               state = CA;
           }else{
               state = FORWARD;
           }
           break;
           case CA:
           if(c == 'P'){
               state = CAP;
           }else{
               state = FORWARD;
           }
           break;
           case CAP:
           if(c == 'A'){
               state = CAPA;
           }else{
               state = FORWARD;
           }
           break;
           case CAPA:
           log(INFO, "CAPA requested");
           state = FORWARD;
           break;
           case DONERETR:
           log(INFO, "RETR was found");
           state = FORWARD;
           break;
           case DONEUSER:
           log(INFO, "user %s tried to login", parse_user(ptr));
           state = FORWARD;
           break;
        }
        i++;
        c = toupper(ptr[i]);
    }
}

bool parse_greeting(char * response, struct selector_key *key)
{

    if(response[0] == '+')
    {
        ATTACHMENT(key)->was_greeted = true;
        return true;
    }
    return false;
}

char * parse_user(char * ptr){
    int i = 5;
    int idx = 0;
    char * buff[1024];
    while (ptr[i] != '\n') {
        buff[idx] = ptr[i];
        i++;
        idx++;
    }
    return buff;
}


//-----------------------------------------------------------------------------------------------------------------
//                                      EXTERN COMMAND
//-----------------------------------------------------------------------------------------------------------------
static void
extern_cmd_init(const unsigned state, struct selector_key *key) {
    struct extern_cmd * extern_cmd = &ATTACHMENT(key)->extern_cmd;

    extern_cmd->done_read    = false;
    extern_cmd->done_write   = false;
    extern_cmd->error_write  = false;
    extern_cmd->error_read   = false;

    extern_cmd->send_bytes_write    = 0;
    extern_cmd->send_bytes_read     = 0;

    extern_cmd->read_buffer  = &ATTACHMENT(key)->write_buffer;
    extern_cmd->write_buffer = &ATTACHMENT(key)->extern_read_buffer;
    extern_cmd->cmd_rb       = &ATTACHMENT(key)->extern_read_buffer;
    extern_cmd->cmd_wb       = &ATTACHMENT(key)->write_buffer;

    extern_cmd->origin_fd    = &ATTACHMENT(key)->origin_fd;
    extern_cmd->client_fd    = &ATTACHMENT(key)->client_fd;
    extern_cmd->read_fd  = &ATTACHMENT(key)->extern_read_fd;
    extern_cmd->write_fd = &ATTACHMENT(key)->extern_write_fd;

    char *          ptr;
    size_t          count;
    buffer  *       buff  = extern_cmd->write_buffer;
    const char err_msg[] = "extern_cmd_init(), extern command error";
    ptr = (char*) buffer_write_ptr(buff, &count);

    extern_cmd->status = socket_forwarding_cmd(key, opt.cmd);

    if (extern_cmd->status == CMD_STATUS_ERROR) {
        log(ERROR, err_msg)
        buffer_write_adv(buff, strlen(err_msg));
        selector_set_interest(key->s, *extern_cmd->client_fd, OP_WRITE);
    }

    buff = extern_cmd->read_buffer;

    // TODO parsing del mail aca ?
}

static void
extern_cmd_close(const unsigned state, struct selector_key * key) {
    struct extern_cmd * extern_cmd  = &ATTACHMENT(key)->extern_cmd;

    selector_unregister_fd(key->s, *extern_cmd->read_fd);
    close(*extern_cmd->read_fd);

    selector_unregister_fd(key->s, *extern_cmd->write_fd);
    close(*extern_cmd->write_fd);
}

static unsigned
extern_cmd_read(struct selector_key *key) {
   // TODO read
    return 0;
}

static unsigned
extern_cmd_write(struct selector_key * key) {
    // TODO write
    return 0;
}

//-----------------------------------------------------------------------------------------------------------------
//                                               MAIN
//-----------------------------------------------------------------------------------------------------------------
static address_data origin_data;
int
main(const int argc, char **argv) {
    appname = *argv;
    parse_options(argc, argv, &opt);
    /* print options just for debug */
    log(INFO,"fstderr       = %s\n", opt.fstderr);
    log(INFO,"local_port    = %d\n", opt.local_port); // local port to listen connections
    log(INFO,"origin_port   = %d\n", opt.origin_port);
    log(INFO,"mgmt_port     = %d\n", opt.mgmt_port);
    log(INFO,"mgmt_addr     = %s\n", opt.mgmt_addr);
    log(INFO,"pop3_addr     = %s\n", opt.pop3_addr);
    log(INFO,"origin_server = %s\n", opt.origin_server); // listen to a specific interface
    log(INFO,"cmd           = %s\n", opt.cmd);

    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct sockaddr_in addr;
    struct sockaddr_in mngmt_addr;
    socklen_t admin_addr_length;

    metrics = init_metrics();

    int proxy_fd = create_proxy_socket(addr, opt);
    int admin_fd = create_management_socket(mngmt_addr, opt);

    if(proxy_fd == -1 || admin_fd == -1)
    {
        goto finally;
    }


    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if(selector_fd_set_nio(proxy_fd) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    if(selector_fd_set_nio(admin_fd) == -1) {
        err_msg = "getting admin socket flags";
        goto finally;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    const struct fd_handler passive_accept_handler = {
        .handle_read       = proxy_tcp_connection,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };
    const struct fd_handler passive_admin_handler = {
        .handle_read       = admin_connection,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };


    ss = selector_register(selector, proxy_fd, &passive_accept_handler,
                                              OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    ss = selector_register(selector, admin_fd, &passive_admin_handler,
                                              OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering admin_fd";
        goto finally;
    }
    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();


    free_metrics(metrics);

    if(proxy_fd >= 0) {
        close(proxy_fd);
    }
    if(admin_fd >= 0) {
        close(admin_fd);
    }
    return ret;
}



