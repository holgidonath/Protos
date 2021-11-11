/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <arpa/inet.h>

#include "buffer.h"
#include "args.h"
#include "stm.h"
#include "../../src/include/logger.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


static enum proxy_states origin_connect(struct selector_key * key);

struct opt opt;

enum proxy_states
{
    RESOLVE_ORIGIN,
    CONNECT, 
    GREETING,
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


typedef struct client
{

    int client_fd;
    struct copy copy;


} client;

typedef struct origin 
{
    int origin_fd;
    struct sockaddr_storage origin_addr;
    socklen_t origin_addr_len;
    struct addrinfo *origin_resolution;
    struct addrinfo *origin_resolution_current;
    struct copy copy;


} origin;

struct connection 
{
    client client;
    origin origin;
    buffer read_buffer, write_buffer;
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    struct state_machine stm;
};

#define ATTACHMENT(key) ( ( struct connection * )(key)->data)


static void
copy_init(const unsigned state, struct selector_key *key);

static unsigned
copy_r(struct selector_key *key);

static unsigned
copy_w(struct selector_key *key);

static unsigned
connection_ready(struct selector_key  *key);

static const struct state_definition client_statbl[] = 
{
    {
        .state = RESOLVE_ORIGIN,

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

//---------------------------------------------------------------------------------------



//       HANDLERS QUE EMITEN LOS EVENTOS DE LA MAQUINA DE ESTADOS


//------------------------------------------------------------------------------------------

static void proxy_read   (struct selector_key *key);
static void proxy_write  (struct selector_key *key);
static void proxy_block  (struct selector_key *key);
static void proxy_close  (struct selector_key *key);
static void proxy_done  (struct selector_key *key);
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

proxy_close(struct selector_key *key) {

//   proxy_destroy(ATTACHMENT(key));

}


static void

proxy_done(struct selector_key* key) {

  const int fds[] = {

    ATTACHMENT(key)->client.client_fd,

    ATTACHMENT(key)->origin.origin_fd,

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
        
        con->origin.origin_fd = -1;
        con->client.client_fd = client_fd;

        con->stm    .initial = RESOLVE_ORIGIN;
        con->stm    .max_state = PERROR;
        con->stm    .states = proxy_describe_states();
        stm_init(&con->stm);

        buffer_init(&con->read_buffer, N(con->raw_buff_a), con->raw_buff_a);
        buffer_init(&con->write_buffer, N(con->raw_buff_b), con->raw_buff_b);
    }
    return con;
}

void
origin_connection(struct selector_key *key)
{
    int origin = 0, valread;
    struct sockaddr_in serv_addr;
    char *hello = "Hello from client";
    char buffer[1024] = {0};
    if ((origin = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        goto fail;
    }
    if(selector_fd_set_nio(origin) == -1) {
        goto fail;
    }



    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(opt.origin_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, opt.origin_server, &serv_addr.sin_addr)<=0)
    {
        goto fail;
    }

    if (connect(origin, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        if(errno == EINPROGRESS) {

      // es esperable, tenemos que esperar a la conexión


      // dejamos de de pollear el socket del cliente

      selector_status st = selector_set_interest_key(key, OP_NOOP);

      if(SELECTOR_SUCCESS != st) {



        //goto fail;

      }


      // esperamos la conexion en el nuevo socket

      st = selector_register(key->s, origin, NULL,

                   OP_WRITE, NULL);

      if(SELECTOR_SUCCESS != st) {



        //goto fail;

      }



    } else {



      //goto fail;

    }

    }
    send(origin , hello , strlen(hello) , 0 );
    printf("Hello message sent\n");
    valread = read( origin , buffer, 1024);
    printf("%s\n",buffer );
    return;

fail:
    if(origin != -1) {
        close(origin);
    }
}

static unsigned connection_ready(struct selector_key  *key) 
{
	return COPY;
}

static unsigned origin_connect(struct selector_key * key) {
    enum proxy_states stm_next_status = COPY;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
//    ATTACHMENT(key)->origin.origin_fd = sock;
    if (sock < 0) {
        perror("socket() failed");
        return PERROR;
    }
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(opt.origin_port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, opt.origin_server, &serv_addr.sin_addr)<=0)
    {
        goto error;
    }

    if (selector_fd_set_nio(sock) == -1) {
        goto error;
    }

//    if (connect(sock,
//                (const struct sockaddr *)&ATTACHMENT(key)->origin.origin_addr,
//                ATTACHMENT(key)->origin.origin_addr_len
//    ) == -1
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1
            ) {
        if (errno == EINPROGRESS) {
//            selector_status st = selector_set_interest_key(key, OP_NOOP);
//            if (SELECTOR_SUCCESS != st) {
//                goto error;
//            }

        	selector_status st = selector_register(key->s, sock, &proxy_handler, OP_WRITE, key->data);
            if (SELECTOR_SUCCESS != st) {
                goto error;
            }

           // ATTACHMENT(key)->references += 1;

        } else {
            goto error;
        }
    } else {
        abort();
    }
    log(INFO, "origin sever connection success.");

    return stm_next_status;

    error:
    stm_next_status = PERROR;
    log(ERROR, "origin server connection.");
    if (sock != -1) {
        close(sock);
    }

    return stm_next_status;
}


void
proxy_tcp_connection(struct selector_key *key){
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
   

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    struct connection *connection = new_connection(client);
    if(connection == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }

    // memcpy(&con->client->client_addr, &client_addr, client_addr_len);
    // con->client->client_addr_len = client_addr_len;

    // Falta ver todo lo de la STM en struct connection

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &proxy_handler, OP_READ, connection)) {
        goto fail;
    }
//		TODO: falta guardarse en la estructura origin los datos del sv a conectarse
//    if(inet_pton(AF_INET, opt.origin_server, &(((struct sockaddr_in *)(&ATTACHMENT(key)->origin.origin_addr))->sin_addr))<=0)
//	{
//		goto fail;
//	}
//    ATTACHMENT(key)->origin.origin_addr_len = (socklen_t)sizeof(struct sockaddr_in);

    //TODO: habria que hacer algo con lo que retorna esa funcion para la stm
    origin_connect(key);
    

    return ;

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

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
       perror("unable to create management socket");
        return -1;
    }

    fprintf(stdout, "Listening on TCP port %d\n", opt.mgmt_port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("unable to bind management socket");
        return -1;
    }

    if (listen(server, 20) < 0) {
        perror("unable to listen in management socket");
        return -1;
    }

    return server;
}




//-----------------------------------------------------------------------------------------------------------------


//                                          COPY FUNCTIONS


//-----------------------------------------------------------------------------------------------------------------

static void
copy_init(const unsigned state, struct selector_key *key)
{
	log(INFO, "llegamos a copy");
    struct copy * d = &ATTACHMENT(key)->client.copy;

    d->fd       = &ATTACHMENT(key)->client.client_fd;
    d->rb       = &ATTACHMENT(key)->read_buffer;
    d->wb       = &ATTACHMENT(key)->write_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->origin.copy;

    d->fd       = &ATTACHMENT(key)->origin.origin_fd;
    d->rb       = &ATTACHMENT(key)->write_buffer;
    d->wb       = &ATTACHMENT(key)->read_buffer;
    d->duplex   = OP_READ | OP_WRITE;
    d->other    = &ATTACHMENT(key)->client.copy;
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
    struct copy *d = &ATTACHMENT(key)->client.copy;

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

    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b       = d->rb;
    unsigned ret    = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
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

    assert(*d->fd == key->fd);

    size_t size;
    ssize_t n;

    buffer* b       = d->wb;
    unsigned ret    = COPY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
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


//                                               MAIN


//-----------------------------------------------------------------------------------------------------------------




int
main(const int argc, const char **argv) {
    unsigned port = 1080;
    parseOptions(argc, argv, &opt);
    // if(argc == 1) {
    //     // utilizamos el default
    // } else if(argc == 2) {
    //     char *end     = 0;
    //     const long sl = strtol(argv[1], &end, 10);

    //     if (end == argv[1]|| '\0' != *end 
    //        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
    //        || sl < 0 || sl > USHRT_MAX) {
    //         fprintf(stderr, "port should be an integer: %s\n", argv[1]);
    //         return 1;
    //     }
    //     port = sl;
    // } else {
    //     fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    //     return 1;
    // }

    // no tenemos nada que leer de stdin
    close(0);

    
    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;


  
    struct sockaddr_in addr;
    struct sockaddr_in mngmt_addr;

    int proxy_fd = create_proxy_socket(addr, opt);
    // int admin_fd = create_management_socket(mngmt_addr,opt);

    if(proxy_fd == -1 ) //o admin_fd
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
    ss = selector_register(selector, proxy_fd, &passive_accept_handler,
                                              OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
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

    // socksv5_pool_destroy();

    if(proxy_fd >= 0) {
        close(proxy_fd);
    }
    return ret;
}



