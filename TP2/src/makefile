PROXY_NAME = pop3filter
PROXY_SOURCES = proxytcp buffer args selector stm util metrics
LIBS =  logger
CC_C = gcc
PTHREADFLAG = -pthread
CFLAGS = -Wall -fsanitize=address -g -Wno-unused-variable -Wno-unused-but-set-variable

all: proxy-server

clean:
	- rm -f *.o  pop3filter

proxy-server:
	$(CC_C) $(CFLAGS) $(PROXY_SOURCES:=.c) $(LIBS:=.c) -o $(PROXY_NAME) $(PTHREADFLAG)

.PHONY: clean all
