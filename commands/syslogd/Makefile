##
##  @(#)Makefile	1.00	Jan. 11, 2000
##
##  Makefile for syslogd/klogd
##  $Id$

BINDIR	= /usr/bin

CC	= exec cc
RM	= rm -f
EXTRA	= -DPIDFILE=\"/var/run/syslogd.pid\"
CPPFLAGS= -D_MINIX -D_POSIX_SOURCE -Ddebug=0 $(EXTRA) -I.
CFLAGS	= -m -O $(CPPFLAGS)
MKDEP	= mkdep "$(CC) -E $(CPPFLAGS)"
LDFLAGS = -i -o $@
TARGETS = logger syslogd

SRCS =	logger.c syslog.c syslog_test.c syslogd.c
OBJS =	logger.o syslog.o syslog_test.o syslogd.o

all: $(TARGETS)	

syslogd:	syslogd.o
	$(CC) $? $(LDFLAGS)
	@install -S 8kw $@

syslog_test: syslog_test.o syslog.o
	$(CC) syslog_test.o syslog.o $(LDFLAGS)
	@install -S 2kw $@

logger: logger.o syslog.o
	$(CC) logger.o syslog.o $(LDFLAGS)
	@install -S 2kw $@

clean:
	$(RM) $(OBJS) $(TARGETS) syslog_test *.BAK core

install:	$(BINDIR)/logger $(BINDIR)/syslog_test $(BINDIR)/syslogd
$(BINDIR)/logger:	logger
	install -cs -m 755 -o bin -g operator $? $@
$(BINDIR)/syslog_test:	syslog_test
	install -cs -m 755 -o bin -g operator $? $@
$(BINDIR)/syslogd:	syslogd
	install -cs -m 700 -o root -g operator $? $@
