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
#define ARGUMENTS           53


struct opt opt;

const char *appname;

static metrics_t metrics;

int should_parse;
int should_parse_retr;
bool capa_found = false;
bool has_pipelining = true;
bool has_written = false;
bool greeted = true;

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

static fd_interest
filter_compute_interest(fd_selector s, struct copy * copy, struct data_filter * data_filter);

static unsigned
connection_ready(struct selector_key  *key);

static unsigned
resolve_done(struct selector_key * key);

static void *
resolve_blocking(void * data);

static struct copy *
get_copy_ptr(struct selector_key *key);

static void
socket_forwarding_cmd (struct selector_key * key, char *cmd);

static void
extern_cmd_finish(struct selector_key *key);

int parse_command(char * ptr);
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

/* Delego el chequeo del fd para saber que struct copy usar */
static struct copy *
get_copy_ptr(struct selector_key *key) {
    struct connection * conn = ATTACHMENT(key);
    struct copy * ptr = NULL;

    if(key->fd == conn->client_fd) {
        ptr = &conn->copy_client;
    }
    else if(key->fd == conn->origin_fd) {
        ptr = &conn->copy_origin;
    }
    else {
        ptr =  &conn->copy_filter;
    }
    if(ptr == NULL) {
        log(ERROR, "get_copy_ptr: NULL pointer return for fd=%s", key->fd);
    }
    return ptr;
}

static void
copy_init(const unsigned state, struct selector_key *key)
{
    struct copy * d = &ATTACHMENT(key)->copy_client;

    // Init copy client struct
    d->fd       = &ATTACHMENT(key)->client_fd;
    d->rb       = &ATTACHMENT(key)->read_buffer;
    d->wb       = &ATTACHMENT(key)->write_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->copy_origin;
    d->target   = COPY_CLIENT;

    // Init copy origin struct
    d			= &ATTACHMENT(key)->copy_origin;
    d->fd       = &ATTACHMENT(key)->origin_fd;
    d->rb       = &ATTACHMENT(key)->write_buffer;
    d->wb       = &ATTACHMENT(key)->read_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->copy_client;
    d->target   = COPY_ORIGIN;

    // Init coy filter struct
    d			= &ATTACHMENT(key)->copy_origin;
    d->rb       = &ATTACHMENT(key)->filter_buffer;
    d->wb       = &ATTACHMENT(key)->write_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->copy_filter;
    d->target   = COPY_FILTER;

}

static fd_interest
copy_compute_interests_origin(fd_selector s, struct  copy* d)
{
    fd_interest ret = OP_NOOP;
    if((d->duplex & OP_READ) && buffer_can_write(d->rb) && has_written)
    {
        ret |= OP_READ;
    }
    if((d->duplex & OP_WRITE) && buffer_can_read(d->wb) && !has_written)
    {
        ret |= OP_WRITE;
    }
    if(SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret))
    {
        abort();
    }
    return ret;
}

static fd_interest
copy_compute_interests_client(fd_selector s, struct  copy* d)
{
    fd_interest ret = OP_NOOP;
    if((d->duplex & OP_READ) && buffer_can_write(d->rb) && !has_written)
    {
        ret |= OP_READ;
    }
    if((d->duplex & OP_WRITE) && buffer_can_read(d->wb) && has_written)
    {
        ret |= OP_WRITE;
    }
    if(SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret))
    {
        abort();
    }
    return ret;
}

static fd_interest
filter_compute_interest(fd_selector s, struct copy * copy, struct data_filter * data_filter) {

    fd_interest retWrite = OP_NOOP;
    fd_interest retRead  = OP_NOOP;

    if(data_filter->state == FILTER_FILTERING) {
        if(buffer_can_read(copy->wb)) // para saber si vino el .\r\n
            retWrite = WRITE;
        if(SELECTOR_SUCCESS != selector_set_interest(s, data_filter->fdin[WRITE], retWrite))
        log(ERROR, "Problem trying to set interest: %d, to selector in filter, in pipe.", retWrite);
    }

    if(buffer_can_write(copy->rb)) {
        retRead = READ;
    }

    if(SELECTOR_SUCCESS != selector_set_interest(s, data_filter->fdout[READ], retRead) {
        log(FATAL, "Problem trying to set interest: %d, to selector in filter, out pipe.", retRead);
        abort();
    }

    log(INFO, "Setting filter interest: WRITE: %d y READ %d.", retWrite, retRead);
    return retRead | retWrite;
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

int
send_to_origin(size_t command, uint8_t *ptr, struct selector_key *key, struct copy* d)
{
    int n;
    log(INFO, "command:%d",command);
    n = send(key->fd, ptr, command, MSG_NOSIGNAL);
    has_written = true;
    copy_compute_interests_origin(key->s, d);
    copy_compute_interests_client(key->s, d->other);
    return n;
}

ssize_t
send_to_client(uint8_t *ptr, size_t  size, struct selector_key *key,struct copy* d)
{
    int n;
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    copy_compute_interests_client(key->s, d);
    copy_compute_interests_origin(key->s, d->other);
    return n;
}


void check_if_pipe_present(char * ptr, buffer * b);
static unsigned
copy_r(struct selector_key *key){
    log(DEBUG, "==== COPY_R ====");

    unsigned ret = COPY; // starting state for interaction
    struct connection * conn = ATTACHMENT(key);
    struct copy * d          = copy_ptr(key);
    has_written = false;

    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b = d->rb; // read buffer for current data struct
    uint8_t *ptr = buffer_write_ptr(b, &size);

    n = recv(key->fd, ptr, size, 0);

    if( n > 0 )
    {
        buffer_write_adv(b,n);
        if(key->fd == conn->origin_fd)
        {
            has_written = true;
            log(DEBUG, "READING FROM ORIGIN:%s", ptr);
            if(should_parse_retr){
                log(INFO, "parse retr");
                if (opt.cmd)
                {
                    socket_forwarding_cmd(key,opt.cmd);
                    buffer_reset(b);
                    n = recv(conn->extern_cmd.pipe_out[WRITE], ptr, size, 0);
                    buffer_write_adv(b,n);
                }
                should_parse_retr = 0;
                //ret = FILTER;
            }else{
                log(INFO, "dont parse retr");
            }
            copy_compute_interests_origin(key->s, d);
            copy_compute_interests_client(key->s, d->other);

        }
        else if(key->fd == conn->client_fd)
        {

            log(DEBUG, "READING FROM CLIENT");
            copy_compute_interests_client(key->s, d);
            copy_compute_interests_origin(key->s, d->other);

        }

    } else {
        log(ERROR, "copy_r: failed to read");
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~OP_READ;
        if(*d->other->fd != -1)
        {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~OP_WRITE;
        }
        ret = DONE;
    }

        if(d->duplex == OP_NOOP) {
            ret = DONE;
        }

    return ret;
}

static unsigned
copy_w(struct selector_key *key)
{
    log(DEBUG, "==== COPY_W ====");
    struct connection *conn = ATTACHMENT(key);
    struct extern_cmd *filter = (struct extern_cmd *) &ATTACHMENT(key)->extern_cmd;
    struct copy *d = copy_ptr(key);
    assert(*d->fd == key->fd);
    int command = 0;
    size_t size;
    ssize_t n;

    buffer* b       = d->wb;
    unsigned ret    = COPY;


    uint8_t *ptr = buffer_read_ptr(b, &size);

    if(key->fd == conn->origin_fd && size > 1)
    {
        log(DEBUG, "WRITING TO ORIGIN");

           command = parse_command(ptr);
           log(INFO, "command:%d", command);
           has_written = true;
           n = send_to_origin(command,ptr, key, d);

    }
    else if(key->fd == conn->origin_fd && size == 0)
    {
        log(DEBUG, "WRITING 0 TO ORIGIN");
        has_written = true;
        n = send_to_origin(size,ptr, key, d);
    }
    else if(key->fd == conn->client_fd)
    {

        log(DEBUG, "WRITING TO CLIENT");
        has_written = false;
        if(capa_found)
        {
            check_if_pipe_present(ptr, b);
            if(!has_pipelining){
                size += 12;
            }
            buffer_write_adv(b, 12);
            capa_found = false;
        }
        n = send_to_client(ptr,size,key,d);

    }

//    else if(key->fd == conn->client_fd && capa_found)
//    {
//        check_if_pipe_present(ptr, b);
//        capa_found = false;
//        if(!has_pipelining){
//            size += 12;
//        }
//        n = send(key->fd, ptr, size, MSG_NOSIGNAL);
//    }

    if( n < 0 ) {
        ret = PERROR;
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~OP_WRITE;
        if(*d->other->fd != -1) {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~OP_READ;
        }
        ret = DONE;
    }
    else {
        conn->has_filtered_mail = false;
        buffer_read_adv(b,n);
    }

    if(d->duplex == OP_NOOP)
    {
        ret = DONE;
    }

    return ret;
}


static void
extern_cmd_finish(struct selector_key *key)
{
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
int parse_command(char * ptr)
{
    int i = 0;
    int args_i = 0;
    int state = BEGIN;
    char args_buff[2048] = {0};
    char c = toupper(ptr[0]);
    while(state != DONEPARSING) {
        log(INFO, "%c", c);
        switch (state) {
            case BEGIN:
                if(c == 'R'){
                    state = R;
                }else if(c == 'U'){
                    state = U;
                }else if(c == 'C'){
                    state = C;
                }else if(c == 'P'){
                    state = P;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }
                else
                {
                    state = GOTORN;
                }
                break;

            case U:
                if (c == 'S') {
                    state = US;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case US:
                if (c == 'E') {
                    state = USE;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case USE:
                if (c == 'R') {
                    state = USER;

                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case USER:
                if (c == ' ') {
                    state = ARGUMENTS;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case P:
                if (c == 'A') {
                    state = PA;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case PA:
                if (c == 'S') {
                    state = PAS;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case PAS:
                if (c == 'S') {
                    state = PASS;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case PASS:
                if (c == ' ') {
                    state = ARGUMENTS;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                } else {
                    state = GOTORN;
                }
                break;
            case R:
                if(c == 'E'){
                    state = RE;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }else{
                    state = GOTORN;
                }
                break;
            case RE:
                if(c == 'T'){
                    state = RET;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }else{
                    state = GOTORN;
                }
                break;
            case RET:
                if(c == 'R'){
                    state = RETR;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }
                else
                {
                    state = GOTORN;
                }
                break;
            case RETR:
                if(c == ' ') {
                    state = ARGUMENTS;
                    should_parse_retr = 1;
                }
                else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }
                else
                {
                    state = GOTORN;
                }
                break;
            case C:
                if(c == 'A'){
                    state = CA;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }else{
                    state = GOTORN;
                }
                break;
            case CA:
                if(c == 'P'){
                    state = CAP;
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }else{
                    state = GOTORN;
                }
                break;
            case CAP:
                if(c == 'A'){
                    state = CAPA; //TODO: aca hay que checkear antes de decir que encontramos el CAPA que lo que siga sea \r\n (creo que asi especifica pop3 que termina cada linea, sino ver RFC)
                } else if(c == '\r')
                {
                    state = CONTRABARRAR;
                }else{
                    state = GOTORN;
                }
                break;
            case CAPA:
                if(c == '\r'){
                    //log(INFO, "new line found");
                    state = CONTRABARRAR;
                    capa_found = true;
                }else{
                    state = GOTORN;
                }
                break;
            case ARGUMENTS:
                if (c == '\r')
                {
                    state = CONTRABARRAR;
                }
                else {
                    state = ARGUMENTS;
                    args_buff[args_i++] = ptr[i];
                }
                break;

            case CONTRABARRAR:
                if (c == '\n') {
                    log(INFO, "new line found");
                    state = DONEPARSING;
                }
//                else {
//                    state = GOTORN;
//                }
                break;

            case GOTORN:
                while(c != '\r' && c != '\n')
                {
                    log(INFO,"%c",c);
                    i++;
                    c = toupper(ptr[i]);
                }
                state = CONTRABARRAR;
                break;
        }


        i++;
        c = toupper(ptr[i]);

    }
    return i;
}


int parse_command_old(char * ptr, int n){
    int i = 0;
    int state = BEGIN;
    int rsp = 0;
    char c = toupper(ptr[0]);
    while(state != DONEPARSING)
    {
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
    return i;
}


void check_if_pipe_present(char * ptr, buffer *b){
    int i = 0;

    int state = BEGIN_P;
    while (ptr[i] != '.')
    {
        switch (state){
            case BEGIN_P:
            if(ptr[i] == 'P'){
                state=P;
            }else{
                state = BEGIN_P;
            }
            break;
            case P:
            if(ptr[i] == 'I'){
                state=PI;
            }else{
                state = BEGIN_P;
            }
            break;
            case PI:
            if(ptr[i] == 'P'){
                state=PIP;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIP:
            if(ptr[i] == 'E'){
                state=PIPE;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPE:
            if(ptr[i] == 'L'){
                state=PIPEL;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPEL:
            if(ptr[i] == 'I'){
                state=PIPELI;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPELI:
            if(ptr[i] == 'N'){
                state=PIPELIN;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPELIN:
            if(ptr[i] == 'I'){
                state=PIPELINI;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPELINI:
            if(ptr[i] == 'N'){
                state=PIPELININ;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPELININ:
            if(ptr[i] == 'G'){
                state=PIPELINING;
            }else{
                state = BEGIN_P;
            }
            break;
            case PIPELINING:
                if(ptr[i] == '\r'){
                    has_pipelining = true;
                    log(INFO, "Origin supports pipelining");
                }

            break;
        }

    i++;
    }
    if(state != PIPELINING){
        has_pipelining = false;
        log(INFO, "Origin does not support pipelining");
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
        ptr[i++] = '\r';
        ptr[i++] = '\n';
        ptr[i++] = '.';
        ptr[i++] = '\r';
        ptr[i++] = '\n';
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
filter_init (struct selector_key * key, char *cmd) {
    log(DEBUG, "==== FILTER_INIT ====");

    int n;
    struct connection * conn = ATTACHMENT(key);
    struct data_filter * filter =  &conn->data_filter;

    // filter_in : father --> child
    int *filter_in = filter->fdin;

    // filter_out: child  --> father
    int *filter_out = filter->fiout;

    // initialize fds just in case
    for(int i = 0; i < 2; i++) {
        filter_in[i]  = -1;
        filter_out[i] = -1;
    }

    // flush filter buffer
    buffer_reses(conn->flter_buffer);

    // create filter input and output pipes
    if (pipe(filter_in) < 0 || pipe(filter_out) < 0) {
        log(FATAL, "filter_init: Cannot create pipe");
        exit(EXIT_FAILURE);
    }

    // forking to attend process
    pid_t pid = fork();
    if( pid == 0 ) {
        filter->pid_child = -1;
        dup2(pipe_in[READ], STDIN_FILENO); // stdin --> filter_in[READ]
        dup2(pipe_out[WRITE], STDOUT_FILENO); // stdout --> filter_out[WRITE]
        close(pipe_in[WRITE]);
        close(pipe_out[READ]);

        // setting custom sderr
        int fderr = open(opt->fstderr, O_WRONLY | O_APPEND);
        if(fderr > 0) {
            log(INFO, "New stderr log filepath: %s", opt->fstderr)
            dup2(fderr, STDERR_FILENO);

        } else {
            log(ERROR, "filter_init: Couldn't open %s", opt->fstderr);
            log(INFO, "filter stderr filepath stays at: /dev/null");
        }
        // setting environment variable for child process
        env_var_init(username);

        // executing command
        system(cmd);
        // read all stdin and write it stdout
        readin_writeout();

        // change filter state
        filter->state = FILTER_ENDING;
        filter_destroy(key);

    } else {
        filter->pid_child = pid;

        close(filter_in[READ]);
        filter->fdin[READ]   = -1;
        close(filter_out[WRITE]);
        filter->fdout[WRITE] = -1;

        selector_status ss = SELECTOR_SUCCESS;
        ss |= selector_register(key->s,
                                filter_in[WRITE],
                                &proxy_handler,
                                OP_NOOP,
                                conn);

        ss |= selector_register(key->s,
                                filter_out[READ],
                                &proxy_handler,
                                OP_NOOP,
                                conn);

        selector_fd_set_nio(pipe_in[WRITE]);
        selector_fd_set_nio(pipe_out[READ]);
    }
}

static void
readin_writeout() {
    ssize_t n;
    uint8_t dataBuffer[CHILD_BUFFER_SIZE];

    do {
        n = read(STDIN_FILENO, dataBuffer, sizeof(dataBuffer));
        if( n > 0 ) {
            write(STDOUT_FILENO, dataBuffer, n);
        }
    } while( n > 0 );
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



