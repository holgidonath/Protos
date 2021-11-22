/**

 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h> // toupper
#include <linux/sctp.h>

#include "include/buffer.h"
#include "include/args.h"
#include "include/stm.h"
#include "include/logger.h"
#include "include/main.h"
#include "include/util.h"
#include "include/metrics.h"
#include "include/proxyadmin.h"
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
#define GOTORN              35
#define BEGIN_P             36
#define P                   37
#define PI                  38
#define PIP                 39
#define PIPE                40
#define PIPEL               41
#define PIPELI              42
#define PIPELIN             43
#define PIPELINI            44
#define PIPELININ           45
#define PIPELINING          46
#define PA                  47
#define PAS                 48
#define PASS                49
#define CONTRABARRAR        50
#define NEWLINE             51
#define DONEPARSING         52


struct opt opt;

const char *appname;

static metrics_t metrics;

int should_parse;
int should_parse_retr;

char * get_stats(void)
{
    char * to_ret = malloc(100); //TODO: free este string
    sprintf(to_ret, "+Total connections: %lu\nConcurrent connections: %lu\nTotal bytes transferred: %lu\n", metrics->total_connections, metrics->concurrent_connections, metrics->bytes_transfered);
    return to_ret;
}
bool
ending_crlf_dot_crlf(buffer *b);

static struct connection * connections = NULL;

/* ==================================================== */
/*                STATIC PROTOTYPES                     */
/* ==================================================== */
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
filter_init(const unsigned state, struct selector_key *key);

static unsigned
filter_send(struct selector_key *key);

static unsigned
filter_recv(struct selector_key *key);

static void
socket_forwarding_cmd (struct selector_key * key, char *cmd);

static void
extern_cmd_finish(struct selector_key *key);

int parse_command(char * ptr, int n);
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
        .state            = FILTER,
        .on_arrival       = filter_init,
        .on_read_ready    = filter_recv,
        .on_write_ready   = filter_send,
    },
    {
        .state = COPY,
        .on_arrival = copy_init,
        .on_read_ready = copy_r,
        .on_write_ready = copy_w,
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
        con->was_retr = false;
        con->has_filtered_mail = false;
        con->read_all_mail = false;
        con->filtered_all_mail = false;
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
        //log(INFO, "con->fd = %d         key->fd = %d\n", con->origin_fd, key->fd);
        if (getsockopt(con->origin_fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            if (error != 0) {
                //log(INFO, "ooooo");
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
//        log(INFO, "con->fd = %d         key->fd = %d\n", con->origin_fd, key->fd);
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

    struct sctp_initmsg initmsg;
    memset (&initmsg, 0, sizeof (initmsg));
    initmsg.sinit_num_ostreams = 5;
    initmsg.sinit_max_instreams = 5;
    initmsg.sinit_max_attempts = 4;

    setsockopt(admin, IPPROTO_SCTP,SCTP_INITMSG, &initmsg, sizeof(initmsg));

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
void check_if_pipe_present(char * ptr, buffer * b);
static unsigned
copy_r(struct selector_key *key) {
    log(DEBUG, "==== COPY_R ====");
    struct copy *d = copy_ptr(key);
    assert(*d->fd == key->fd);

    int command_state;

    size_t size;
    ssize_t n;

    buffer* b       = d->rb;
    unsigned ret    = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    buffer_write_adv(b,n);

    if( n > 0 ) {
        log(INFO, "str length is %d", n);
        metrics->bytes_transfered += n;

        if(ptr[0] == '+'){
            log(INFO, "Response from server:\n%s", ptr);
            if (should_parse)
            {
                log(INFO, "it was a capa response\n");
                check_if_pipe_present(ptr, b);
                should_parse = 0;
            }
            if(should_parse_retr){
                log(INFO, "it was a retr response\n");
                should_parse_retr = 0;
            }
        }

        if(key->fd == ATTACHMENT(key)->client_fd){
            if(ptr[0] == 'R'){
                log(INFO,"possible retr found");
            }
            command_state = parse_command(ptr, n);
            if(command_state == DONERETR) {
                ATTACHMENT(key)->was_retr = true; // el commnado fue un RETR
                if(opt.cmd) {
                    log(INFO, "copy_r: Going through an external command...");
                    ATTACHMENT(key)->read_all_mail = ending_crlf_dot_crlf(b);
                    socket_forwarding_cmd(key, opt.cmd);
                    selector_status ss = SELECTOR_SUCCESS;
                    selector_register(key->s,
                                      ATTACHMENT(key)->w_to_filter_fds[WRITE],
                                      &proxy_handler,
                                      OP_WRITE,
                                      key->data);
                    selector_fd_set_nio(ATTACHMENT(key)->w_to_filter_fds[WRITE]);
                    ss |= selector_set_interest_key(key, OP_NOOP);
                    ss |= selector_set_interest(key->s,
                                                ATTACHMENT(key)->w_to_filter_fds[WRITE],
                                                OP_WRITE);
                    ret = ss == SELECTOR_SUCCESS ? FILTER : PERROR;
                }
            }
        }

    } else {
        log(ERROR, "copy_r: failed to read");
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~OP_READ;
        if(*d->other->fd != -1){
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~OP_WRITE;
        }
    }

    if( ret != FILTER ) {
        copy_compute_interests(key->s, d);
        copy_compute_interests(key->s, d->other);

        if(d->duplex == OP_NOOP) {
            ret = DONE;
        }
    }

    return ret;
}

static unsigned
copy_w(struct selector_key *key) {
    struct connection *conn = ATTACHMENT(key);
    struct extern_cmd *filter = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
    struct copy *d = copy_ptr(key);
    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b       = d->wb;
    unsigned ret    = COPY;

    if ( conn->has_filtered_mail ) {
        // from external command
        uint8_t *ptr;
        size_t count;
        ptr = buffer_read_ptr(filter->filtered_mail_buffer, &count);
        n = send(conn->client_fd, ptr, count, MSG_NOSIGNAL);
        metrics->bytes_transfered += n;

        if (conn->filtered_all_mail) {
            extern_cmd_finish(key);
        }
        buffer_reset(filter->filtered_mail_buffer);

    } else {
        // send to server
        uint8_t *ptr = buffer_read_ptr(b, &size);

        if(key->fd == conn->client_fd){
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
    }

    if( n == -1 ) {
        ret = PERROR;
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~OP_WRITE;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~OP_READ;
        }

    } else {
        conn->has_filtered_mail = false;
        buffer_read_adv(b,n);
    }

    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if(d->duplex == OP_NOOP) {
        ret = DONE;
    }

    return ret;
}


static void
extern_cmd_finish(struct selector_key *key) {
    struct connection *conn = ATTACHMENT(key);

    close(conn->r_from_filter_fds[READ]);
    selector_unregister_fd(key->s, conn->w_to_filter_fds[WRITE]);
    selector_unregister_fd(key->s, conn->r_from_filter_fds[READ]);
    conn->w_to_filter_fds[READ]     = -1;
    conn->w_to_filter_fds[WRITE]    = -1;
    conn->r_from_filter_fds[READ]   = -1;
    conn->r_from_filter_fds[WRITE]  = -1;
}
//-----------------------------------------------------------------------------------------------------------------
//                                          PARSING AND POSTERIOR HANDLING FUNCTIONS
//-----------------------------------------------------------------------------------------------------------------
int parse_command(char * ptr, int n){
    int i = 0;
    int state = BEGIN;
    int rsp = 0;
    char c = toupper(ptr[0]);
    log(INFO, "%d", sizeof(ptr));
    while(state != DONEPARSING){
        log(INFO, "%c",c);
       switch(state){
           case BEGIN:
           if(c == 'R'){
               state = R;
           }else if(c == 'U'){
               state = U;
           }else if(c == 'C'){
               state = C;
           }else if(c == 'P'){
               state = P;
           }else{
               state = GOTORN;
           }
           break;
           case R:
           if(c == 'E'){
               state = RE;
           }else{
               state = GOTORN;
           }
           break;
           case RE:
           if(c == 'T'){
               state = RET;
           }else{
               state = GOTORN;
           }
           break;
           case RET:
           if(c == 'R'){
               state = RETR;
           }else{
               state = GOTORN;
           }
           break;
           case RETR:
           should_parse_retr = 1;
           state = GOTORN;
           break;
           case U:
           if(c == 'S'){
               state = US;
           }else{
               state = GOTORN;
           }
           break;
           case US:
           if(c == 'E'){
               state = USE;
           }else{
               state = GOTORN;
           }
           break;
           case USE:
           if(c == 'R'){
                parse_user(ptr);
                //hay que ir copiando aca
                state = GOTORN;
           }else{
               state = GOTORN;
           }
           break;
           case USER:
           if(c == ' '){
               parse_user(ptr);
           //hay que ir copiando aca
                state = GOTORN;
           }else{
               state = GOTORN;
           }
           break;
           case P:
           if(c == 'A'){
               state = PA;
           }else{
               state = GOTORN;
           }
           break;
           case PA:
           if(c == 'S'){
               state = PAS;
           }else{
               state = GOTORN;
           }
           break;
           case PAS:
           if(c == 'S'){
               state = GOTORN;
           }else{
               state = GOTORN;
           }
           break;
           case PASS:
           state = GOTORN;
           break;
           case C:
           if(c == 'A'){
               state = CA;
           }else{
               state = GOTORN;
           }
           break;
           case CA:
           if(c == 'P'){
               state = CAP;
           }else{
               state = GOTORN;
           }
           break;
           case CAP:
           if(c == 'A'){
               state = CAPA; //TODO: aca hay que checkear antes de decir que encontramos el CAPA que lo que siga sea \r\n (creo que asi especifica pop3 que termina cada linea, sino ver RFC)
               should_parse = 1;
           }else{
               state = GOTORN;
           }
           break;
           case CAPA:
           if(c == '\r'){
               //log(INFO, "new line found");
               state = CONTRABARRAR;
           }else{
               state = GOTORN;
           }
           break;
           case CONTRABARRAR:
           if(c == '\n'){
               //log(INFO, "new line found");
               log(INFO, "new line found");
                state = BEGIN;
           }else{
               state=GOTORN;
           }
           break;
           case NEWLINE:
           //hacer lo del interest y demas
           log(INFO, "new line found");
           state = BEGIN;
           break;
           case DONERETR:
           log(INFO, "RETR was found");
           rsp = DONERETR;
           state = GOTORN;
           break;
           case DONEUSER:
           parse_user(ptr);
           //hay que ir copiando aca
           state = GOTORN;
           break;
           case GOTORN:
           while(c != '\r'){
               log(INFO,"%c",c);
               i++;
               c = toupper(ptr[i]);
           }
           state = CONTRABARRAR;
           break;
        }
        // i++;
        // c = toupper(ptr[i]);
        if(i < n){
            i++;
            c = toupper(ptr[i]);
        }else{
            state = DONEPARSING;
        }
    }
    return rsp;
}

void check_if_pipe_present(char * ptr, buffer *b){
    int i = 0;
    int parsing_possible_pipe = 0;
    int state = BEGIN_P;
    while (ptr[i] != '.')
    {   
        log(INFO,"%c",ptr[i]);
        if(ptr[i] != 'P' && !parsing_possible_pipe){
            while (ptr[i] != '\n')
            {
                i++;
            } 
        }else{
            parsing_possible_pipe = 1;
            switch (state){
                case BEGIN_P:
                if(ptr[i] == 'P'){
                    state=P;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case P:
                if(ptr[i] == 'I'){
                    state=PI;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PI:
                if(ptr[i] == 'P'){
                    state=PIP;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIP:
                if(ptr[i] == 'E'){
                    state=PIPE;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPE:
                if(ptr[i] == 'L'){
                    state=PIPEL;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPEL:
                if(ptr[i] == 'I'){
                    state=PIPELI;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPELI:
                if(ptr[i] == 'N'){
                    state=PIPELIN;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPELIN:
                if(ptr[i] == 'I'){
                    state=PIPELINI;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPELINI:
                if(ptr[i] == 'N'){
                    state=PIPELININ;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPELININ:
                if(ptr[i] == 'G'){
                    state=PIPELINING;
                }else{
                    parsing_possible_pipe = 0;
                    state = BEGIN_P;
                }
                break;
                case PIPELINING:
                log(INFO, "srv supports pipelining");
                parsing_possible_pipe = 0;
                break;
            }
        }
        i++;  
    }
    if(state != PIPELINING){
        log(INFO, "srv does not support pipelining");
        ptr[i++] = 'P';
        ptr[i++] = 'I';
        ptr[i++] = 'P';
        ptr[i++] = 'E';
        ptr[i++] = 'L';
        ptr[i++] = 'I';
        ptr[i++] = 'N';
        ptr[i++] = 'I';
        ptr[i++] = 'N';
        ptr[i++] = 'G';
        ptr[i++] = '\n';
        ptr[i++] = '.';
        ptr[i++] = '\n';
        buffer_write_adv(b, 13);
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
    char buff[1024] = "";
    while (ptr[i] != '\n') {
        buff[idx] = ptr[i];
        //log(INFO, "%c", buff[idx]);
        i++;
        idx++;
    }
    log(INFO,"User %s tried to login", buff);
    return buff;
}


//-----------------------------------------------------------------------------------------------------------------
//                                      EXTERN COMMAND
//-----------------------------------------------------------------------------------------------------------------
static void
filter_init(const unsigned state, struct selector_key *key) {
    struct extern_cmd * filter = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;

    filter->mail_buffer = &(ATTACHMENT(key)->read_buffer);
    filter->filtered_mail_buffer = &(ATTACHMENT(key)->write_buffer);
}

static unsigned
filter_send(struct selector_key *key) {
    struct extern_cmd *d = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
    enum proxy_states ret;

    size_t   count;
    ssize_t  n;
    buffer  *b = d->mail_buffer;
    uint8_t *ptr;

    ptr = buffer_read_ptr(b, &count);
    n = write(ATTACHMENT(key)->w_to_filter_fds[WRITE], ptr, count);
    if (n == 0) {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
        ret = SELECTOR_SUCCESS == ss ? COPY : PERROR;

    } if(n == -1) {
        log(ERROR, "filter_send: writing to file descriptor failed")
        ret = ERROR;

    } else {
        selector_status ss = SELECTOR_SUCCESS;
        if (ATTACHMENT(key)->read_all_mail){
            close(ATTACHMENT(key)->w_to_filter_fds[WRITE]);
            selector_unregister_fd(key->s, ATTACHMENT(key)->w_to_filter_fds[WRITE]);
            buffer_reset(b);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->r_from_filter_fds[READ], OP_READ);
            ret = ss == SELECTOR_SUCCESS ? FILTER : PERROR;
        } else {
            buffer_reset(b);
            ss |= selector_set_interest_key(key, OP_NOOP);
            ss |= selector_set_interest(key->s, ATTACHMENT(key)->origin_fd, OP_READ);
            ret = SELECTOR_SUCCESS == ss ? COPY : PERROR;
        }
    }

    return ret;
}
bool
ending_crlf_dot_crlf(buffer *b) {

    return *(b->write-1) == '\n' &&
           *(b->write-2) == '\r' &&
           *(b->write-3) == '.'  &&
           *(b->write-4) == '\n' &&
           *(b->write-5) == '\r';
}

bool
is_err_response(buffer* buff){

    return *buff->read     == '-' &&
           *(buff->read+1) == 'E' &&
           *(buff->read+2) == 'R' &&
           *(buff->read+3) == 'R';
}
static unsigned
filter_recv(struct selector_key *key) {
    struct extern_cmd *d = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
    enum proxy_states ret = FILTER;
    size_t  count;
    ssize_t  n;
    uint8_t *ptr;

    buffer  *b = d->filtered_mail_buffer;
    buffer_reset(b);
    ptr = buffer_write_ptr(b, &count);
    n = read(ATTACHMENT(key)->r_from_filter_fds[READ], ptr, count);
    if(n > 0) {
        buffer_write_adv(b, n);
        // chequeo si se leyo el final del mail (CRLF . CRLF)
        ATTACHMENT(key)->read_all_mail = ending_crlf_dot_crlf(b);
        if (ATTACHMENT(key)->read_all_mail){
            close(ATTACHMENT(key)->r_from_filter_fds[READ]);
            selector_unregister_fd(key->s, ATTACHMENT(key)->r_from_filter_fds[READ]);
        }
        ATTACHMENT(key)->has_filtered_mail = true;
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_WRITE);
        ret = ss == SELECTOR_SUCCESS ? COPY : PERROR;

    }
    else if (n == 0) {
        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_set_interest_key(key, OP_NOOP);
        ss |= selector_set_interest(key->s, ATTACHMENT(key)->client_fd, OP_READ);
        ret = ss == SELECTOR_SUCCESS ? FILTER : PERROR;

    }
    else {
        log(ERROR, "filter_recv: reading from file descriptor failed")
        ret = ERROR;
    }

    return ret;
}

/* Forward data from socket 'source' to socket 'destination' by executing the 'cmd' command */
static void
socket_forwarding_cmd (struct selector_key * key, char *cmd) {
    int n;
    // pipe_in (w_to_filter_fds ): father --> child
    // pipe_out (r_from_filter_fds): child  --> father
    int *pipe_in = ATTACHMENT(key)->w_to_filter_fds;
    int *pipe_out = ATTACHMENT(key)->r_from_filter_fds;

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) { // create command input and output pipes
        log(FATAL, "socket_forwarding_cmd: Cannot create pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if( pid == 0 ) {
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

        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_register(key->s,
                                pipe_in[WRITE],
                                &proxy_handler,
                                OP_WRITE,
                                key->data);

        ss |= selector_register(key->s,
                                pipe_out[READ],
                                &proxy_handler,
                                OP_READ,
                                key->data);

        selector_fd_set_nio(pipe_in[WRITE]);
        selector_fd_set_nio(pipe_out[READ]);
    }
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

    should_parse = 0;
    should_parse_retr = 0;
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



