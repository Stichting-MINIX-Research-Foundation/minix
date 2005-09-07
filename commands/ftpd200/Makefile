# Makefile for ftpd
#
# 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
# 2005-02-25 version 2.00

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE -m
LDFLAGS=-i
BINDIR=	/usr/bin
PROG=	in.ftpd
MANDIR= /usr/man/man8
MANPAGE=ftpd.8
CC = exec cc

OBJS=	ftpd.o access.o file.o net.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 8kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG) $(BINDIR)/setup.anonftp $(BINDIR)/ftpdsh

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

$(BINDIR)/setup.anonftp:	setup.anonftp
	install -c -o bin $? $@

$(BINDIR)/ftpdsh:	ftpdsh
	install -c -o bin $? $@

ftpd.o:		ftpd.c   ftpd.h access.h file.h net.h
access.o:	access.c ftpd.h access.h
file.o:		file.c   ftpd.h access.h file.h net.h
net.o:		net.c    ftpd.h                 net.h

installman:	$(MANDIR)/$(MANPAGE)
	cp $(MANPAGE) $(MANDIR)
	echo "You may need to run makewhatis to update man page index"
