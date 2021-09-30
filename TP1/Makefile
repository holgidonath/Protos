.PHONY=clean all
COMPILER=gcc
CFLAGS = -Wall -fsanitize=address -g
all: tcpEchoAddrinfo
clean:	
	- rm -f *.o  server

tcpEchoAddrinfo:      
	$(COMPILER) $(CFLAGS) -o server *.c