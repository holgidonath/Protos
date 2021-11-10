PROXY_NAME = pop3filter
PROXY_SOURCES = proxytcp buffer args selector stm ../../src/logger
CC_C = gcc
PTHREADFLAG = -pthread

all: proxy-server

proxy-server:
	$(CC_C) $(PROXY_SOURCES:=.c) -o $(PROXY_NAME) $(PTHREADFLAG)

.PHONY: all