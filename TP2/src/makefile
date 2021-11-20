PROXY_NAME = pop3filter
PROXY_SOURCES = proxytcp buffer args selector stm util metrics proxyadmin
LIBS =  logger
CC_C = gcc
PTHREADFLAG = -pthread
CFLAGS = -Wall -fsanitize=address -g

all: proxy-server

clean:
	- rm -f *.o  pop3filter

proxy-server:
	$(CC_C) $(CFLAGS) $(PROXY_SOURCES:=.c) $(LIBS:=.c) -lsctp -o $(PROXY_NAME) $(PTHREADFLAG)

.PHONY: clean all
