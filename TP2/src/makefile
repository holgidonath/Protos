PROXY_NAME = pop3filter
ADMIN_CTL_NAME = pop3ctl
PROXY_SOURCES = proxytcp buffer args selector stm util metrics proxyadmin extcmd
ADMIN_CTL_SOURCES = adminctl argsctl
LIBS =  logger
CC_C = gcc
PTHREADFLAG = -pthread
SCTPFLAG	= -lsctp
CFLAGS = -Wall -fsanitize=address -g -Wno-unused-variable -Wno-unused-but-set-variable

all: clean proxy-server admin-ctl

clean:
	@rm -f *.o  $(PROXY_NAME)
	@rm -f *.0	$(ADMIN_CTL_NAME)

proxy-server:
	$(CC_C) $(CFLAGS) $(PROXY_SOURCES:=.c) $(LIBS:=.c) -o $(PROXY_NAME) $(SCTPFLAG) $(PTHREADFLAG)

admin-ctl:
	$(CC_C) $(CFLAGS) $(ADMIN_CTL_SOURCES:=.c) -o $(ADMIN_CLIENT_NAME) $(SCTPFLAG)

.PHONY: all
