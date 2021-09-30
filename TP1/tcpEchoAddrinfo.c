#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>   
#include <arpa/inet.h>    
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> 
#include <ctype.h>
#include "./include/logger.h"
#include "./include/tcpServerUtil.h"
#include "include/echoParser.h"
#include "include/commandParser.h"
#include "include/getParser.h"

#define max(n1,n2)     ((n1)>(n2) ? (n1) : (n2))

#define TRUE   1
#define FALSE  0
#define PORT 9999
#define MAX_SOCKETS 30

#define PORT_UDP 9999
#define MAX_PENDING_CONNECTIONS   3    // un valor bajo, para realizar pruebas


/**
  Se encarga de escribir la respuesta faltante en forma no bloqueante
  */
void handleWrite(int socket, struct buffer * buffer, fd_set * writefds);
/**
  Limpia el buffer de escritura asociado a un socket
  */
void clear( struct buffer * buffer);

/**
  Crea y "bindea" el socket server UDP
  */
int udpSocket(int port);

/**
  Lee el datagrama del socket, obtiene info asociado con getaddrInfo y envia la respuesta
  */
void handleAddrInfo(int socket, int *locale);

int parsePort(char * str){
	int port = atoi(str);
	if(port >= 1024 && port <= 65535){
		return port;
	} else {
		log(ERROR, "Invalid port number, setting to default port");
	}
	return PORT;
}

int main(int argc , char *argv[])
{
	// printf("hola :)\n");
	int port;
	if(argc == 1){
		port = PORT;
	} else if (argc == 2)
	{
		port = parsePort(argv[1]);
	} else {
		log(ERROR, "Invalid arguments! Try only with one argument");
		return -1;
	}
	log(DEBUG, "Server running on port: %d", port);
	int opt = TRUE;
	int master_socket[2];  // IPv4 e IPv6 (si estan habilitados)
	int master_socket_size=0;
	int addrlen , new_socket , client_socket[MAX_SOCKETS] , max_clients = MAX_SOCKETS , activity, i , sd;
	long valread;
	int max_sd;
	struct sockaddr_in address;
	int wasValid[MAX_SOCKETS] = {1};
	int limit[MAX_SOCKETS] = {0};
	int commandParsed[MAX_SOCKETS] = {BEGIN};
	char *commands [] = {"echo", "get"};

	int locale = 'en';

	struct sockaddr_storage clntAddr; // Client address
	socklen_t clntAddrLen = sizeof(clntAddr);

	char buffer[BUFFSIZE + 1];  //data buffer of 1K

	//set of socket descriptors
	fd_set readfds;

	// Agregamos un buffer de escritura asociado a cada socket, para no bloquear por escritura
	struct buffer bufferWrite[MAX_SOCKETS];
	memset(bufferWrite, 0, sizeof bufferWrite);

	// y tambien los flags para writes
	fd_set writefds;

	//initialise all client_socket[] to 0 so not checked
	memset(client_socket, 0, sizeof(client_socket));

	// TODO adaptar setupTCPServerSocket para que cree socket para IPv4 e IPv6 y ademas soporte opciones (y asi no repetor codigo)
	
	// socket para IPv4 y para IPv6 (si estan disponibles)
	///////////////////////////////////////////////////////////// IPv4
	if( (master_socket[master_socket_size] = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
	{
		log(ERROR, "socket IPv4 failed");
	} else {
		//set master socket to allow multiple connections , this is just a good habit, it will work without this
		if( setsockopt(master_socket[master_socket_size], SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
		{
			log(ERROR, "set IPv4 socket options failed");
		}

		//type of socket created
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons( port );

		// bind the socket to localhost port 8888
		if (bind(master_socket[master_socket_size], (struct sockaddr *)&address, sizeof(address))<0) 
		{
			log(ERROR, "bind for IPv4 failed");
			close(master_socket[master_socket_size]);
		}
		else {
			if (listen(master_socket[0], MAX_PENDING_CONNECTIONS) < 0)
			{
				log(ERROR, "listen on IPv4 socket failes");
				close(master_socket[master_socket_size]);
			} else {
				log(DEBUG, "Waiting for TCP IPv4 connections on socket %d\n", master_socket[master_socket_size]);
				master_socket_size++;
			}
		}
	}
	///////////////////////////////////////////////////////////// IPv6
	struct sockaddr_in6 server6addr;
	if ((master_socket[master_socket_size] = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	{
		log(ERROR, "socket IPv6 failed");
	} else {
		if (setsockopt(master_socket[master_socket_size], SOL_SOCKET, SO_REUSEADDR, (char *)&opt,sizeof(opt)) < 0)
		{
			log(ERROR, "set IPv6 socket options failed");
		}
		memset(&server6addr, 0, sizeof(server6addr));
		server6addr.sin6_family = AF_INET6;
		server6addr.sin6_port   = htons(port);
		server6addr.sin6_addr   = in6addr_any;
		if (bind(master_socket[master_socket_size], (struct sockaddr *)&server6addr,sizeof(server6addr)) < 0)
		{
			log(ERROR, "bind for IPv6 failed");
			close(master_socket[master_socket_size]);
		} else {
			if (listen(master_socket[master_socket_size], MAX_PENDING_CONNECTIONS) < 0)
			{
				log(ERROR, "listen on IPv6 failed");
				close(master_socket[master_socket_size]);
			} else {
				log(DEBUG, "Waiting for TCP IPv6 connections on socket %d\n", master_socket[master_socket_size]);
				master_socket_size++;
			}
		}
	}

	// Socket UDP para responder en base a addrInfo
	int udpSock = udpSocket(port);
	if ( udpSock < 0) {
		log(FATAL, "UDP socket failed");
		// exit(EXIT_FAILURE);
	} else {
		log(DEBUG, "Waiting for UDP IPv4 on socket %d\n", udpSock);

	}

	// Limpiamos el conjunto de escritura
	FD_ZERO(&writefds);
	while(TRUE) 
	{
		//clear the socket set
		FD_ZERO(&readfds);

		//add masters sockets to set
		for (int sdMaster=0; sdMaster < master_socket_size; sdMaster++)
			FD_SET(master_socket[sdMaster], &readfds);
		FD_SET(udpSock, &readfds);

		max_sd = udpSock;

		// add child sockets to set
		for ( i = 0 ; i < max_clients ; i++) 
		{
			// socket descriptor
			sd = client_socket[i];

			// if valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET( sd , &readfds);

			// highest file descriptor number, need it for the select function
			if(sd > max_sd)
				max_sd = sd;
		}

		//wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
		activity = select( max_sd + 1 , &readfds , &writefds , NULL , NULL);

		log(DEBUG, "select has something...");	

		if ((activity < 0) && (errno!=EINTR)) 
		{
			log(ERROR, "select error, errno=%d",errno);
			continue;
		}

		// Servicio UDP
		if(FD_ISSET(udpSock, &readfds)) {
			handleAddrInfo(udpSock, &locale);
		}

		//If something happened on the TCP master socket , then its an incoming connection
		for (int sdMaster=0; sdMaster < master_socket_size; sdMaster++) {
			int mSock = master_socket[sdMaster];
			if (FD_ISSET(mSock, &readfds)) 
			{
				if ((new_socket = acceptTCPConnection(mSock)) < 0)
				{
					log(ERROR, "Accept error on master socket %d", mSock);
					continue;
				}

				// add new socket to array of sockets
				for (i = 0; i < max_clients; i++) 
				{
					// if position is empty
					if( client_socket[i] == 0 )
					{
						client_socket[i] = new_socket;
						log(DEBUG, "Adding to list of sockets as %d\n" , i);
						break;
					}
				}
			}
		}

		for(i =0; i < max_clients; i++) {
			sd = client_socket[i];

			if (FD_ISSET(sd, &writefds)) {
				handleWrite(sd, bufferWrite + i, &writefds);
			}
		}

		//else its some IO operation on some other socket :)
		for (i = 0; i < max_clients; i++) 
		{
			sd = client_socket[i];

			if (FD_ISSET( sd , &readfds)) 
			{
				//Check if it was for closing , and also read the incoming message
				if ((valread = read( sd , buffer, BUFFSIZE)) <= 0)
				{
					//Somebody disconnected , get his details and print
					getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
					log(INFO, "Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

					//Close the socket and mark as 0 in list for reuse
					close( sd );
					client_socket[i] = 0;

					FD_CLR(sd, &writefds);
					// Limpiamos el buffer asociado, para que no lo "herede" otra sesión
					clear(bufferWrite + i);
					wasValid[i] = 1;
					limit[i] = 0;
				}
				else {
					log(DEBUG, "Received %zu bytes from socket %d\n", valread, sd);
					// activamos el socket para escritura y almacenamos en el buffer de salida
					FD_SET(sd, &writefds);
					unsigned state = parseCommand(buffer, &commandParsed[i], &valread, &wasValid[i], &limit[i], &bufferWrite[i], locale);
					// valread = bufferWrite[i].len ;
					// int correct = echoParser(buffer,&valread, &wasValid[i], &limit[i]);

					if(state == INVALID || state == INVALID_GET){
						char * error_msg = "Invalid command!\r\n";
						valread = strlen(error_msg);
						bufferWrite[i].buffer = realloc(bufferWrite[i].buffer, bufferWrite[i].len + valread);
						memcpy(bufferWrite[i].buffer + bufferWrite[i].len, error_msg, valread);
						bufferWrite[i].len += valread;
					}
					
					// Tal vez ya habia datos en el buffer
					// TODO: validar realloc != NULL

					
					
					
				}
			}
		}
	}

	return 0;
}

void clear( struct buffer * buffer) {
	free(buffer->buffer);
	buffer->buffer = NULL;
	buffer->from = buffer->len = 0;
}

// Hay algo para escribir?
// Si está listo para escribir, escribimos. El problema es que a pesar de tener buffer para poder
// escribir, tal vez no sea suficiente. Por ejemplo podría tener 100 bytes libres en el buffer de
// salida, pero le pido que mande 1000 bytes.Por lo que tenemos que hacer un send no bloqueante,
// verificando la cantidad de bytes que pudo consumir TCP.
void handleWrite(int socket, struct buffer * buffer, fd_set * writefds) {
	size_t bytesToSend = buffer->len - buffer->from;
	if (bytesToSend > 0) {  // Puede estar listo para enviar, pero no tenemos nada para enviar
		log(INFO, "Trying to send %zu bytes to socket %d\n", bytesToSend, socket);
		size_t bytesSent = send(socket, buffer->buffer + buffer->from,bytesToSend,  MSG_DONTWAIT); 
		log(INFO, "Sent %zu bytes\n", bytesSent);

		if ( bytesSent < 0) {
			// Esto no deberia pasar ya que el socket estaba listo para escritura
			// TODO: manejar el error
			log(FATAL, "Error sending to socket %d", socket);
		} else {
			size_t bytesLeft = bytesSent - bytesToSend;

			// Si se pudieron mandar todos los bytes limpiamos el buffer y sacamos el fd para el select
			if ( bytesLeft == 0) {
				clear(buffer);
				FD_CLR(socket, writefds);
			} else {
				buffer->from += bytesSent;
			}
		}
	} else {
		clear(buffer);
		FD_CLR(socket, writefds);
	}
}

int udpSocket(int port) {

	int sock;
	struct sockaddr_in serverAddr;
	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		log(ERROR, "UDP socket creation failed, errno: %d %s", errno, strerror(errno));
		return sock;
	}
	log(DEBUG, "UDP socket %d created", sock);
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family    = AF_INET; // IPv4cle
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);

	if ( bind(sock, (const struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0 )
	{
		log(ERROR, "UDP bind failed, errno: %d %s", errno, strerror(errno));
		close(sock);
		return -1;
	}
	log(DEBUG, "UDP socket bind OK ");

	return sock;
}


void handleAddrInfo(int socket, int *locale) {
	// En el datagrama viene el nombre a resolver
	// Se le devuelve la informacion asociada

	char buffer[BUFFSIZE];
	unsigned int len, n;

	struct sockaddr_in clntAddr;

	// Es bloqueante, deberian invocar a esta funcion solo si hay algo disponible en el socket    
	n = recvfrom(socket, buffer, BUFFSIZE, 0, ( struct sockaddr *) &clntAddr, &len);
	if ( buffer[n-1] == '\n') // Por si lo estan probando con netcat, en modo interactivo
		n--;
	buffer[n] = '\0';
	log(DEBUG, "UDP received:%s", buffer );
	// TODO: parsear lo recibido para obtener nombre, puerto, etc. Asumimos viene solo el nombre
	printf("%s", buffer);
	int res = udpParseCommand(buffer);
	// // Especificamos solo SOCK_STREAM para que no duplique las respuestas
	// struct addrinfo addrCriteria;                   // Criteria for address match
	// memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
	// addrCriteria.ai_family = AF_UNSPEC;             // Any address family
	// addrCriteria.ai_socktype = SOCK_STREAM;         // Only stream sockets
	// addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

	// Armamos el datagrama con las direcciones de respuesta, separadas por \r\n
	// TODO: hacer una concatenacion segura
	// TODO: modificar la funcion printAddressInfo usada en sockets bloqueantes para que sirva
	//       tanto si se quiere obtener solo la direccion o la direccion mas el puerto
	char bufferOut[BUFFSIZE];
	bufferOut[0] = '\0';

	if (res == 21){
		strcpy(bufferOut, "Invalid command!");
	} else if (res == 22){
		strcpy(bufferOut, "Stats for the server");
	} else if (res == 23){
		*locale = 'en';
		strcpy(bufferOut, "locale was set for english");
	} else if (res == 24){
		*locale = 'es';
		strcpy(bufferOut, "locale was set for spanish");
	}
	
	//strcat(strcpy(bufferOut, "probando el echo "), buffer);
	strcat(bufferOut, "\r\n");
	// struct addrinfo *addrList;
	// int rtnVal = getaddrinfo(buffer, NULL, &addrCriteria, &addrList);
	// if (rtnVal != 0) {
	// 	log(ERROR, "getaddrinfo() failed: %d: %s", rtnVal, gai_strerror(rtnVal));
	// 	strcat(strcpy(bufferOut,"Can't resolve "), buffer);

	// } else {
	// 	for (struct addrinfo *addr = addrList; addr != NULL; addr = addr->ai_next) {
	// 		struct sockaddr *address = addr->ai_addr;
	// 		char addrBuffer[INET6_ADDRSTRLEN];

	// 		void *numericAddress = NULL;
	// 		switch (address->sa_family) {
	// 			case AF_INET:
	// 				numericAddress = &((struct sockaddr_in *) address)->sin_addr;
	// 				break;
	// 			case AF_INET6:
	// 				numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
	// 				break;
	// 		}
	// 		if ( numericAddress == NULL) {
	// 			strcat(bufferOut, "[Unknown Type]");
	// 		} else {
	// 			// Convert binary to printable address
	// 			if (inet_ntop(address->sa_family, numericAddress, addrBuffer, sizeof(addrBuffer)) == NULL)
	// 				strcat(bufferOut, "[invalid address]");
	// 			else {
	// 				strcat(bufferOut, addrBuffer);
	// 			}
	// 		}
	// 		strcat(bufferOut, "\r\n");
	// 	}
	// 	freeaddrinfo(addrList);
	// }
	// Enviamos respuesta (el sendto no bloquea)
	sendto(socket, bufferOut, strlen(bufferOut), 0, (const struct sockaddr *) &clntAddr, len);

	log(DEBUG, "UDP sent:%s", bufferOut );

}

