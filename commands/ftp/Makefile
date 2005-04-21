# Makefile for ftp
#
# 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
#

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i
BINDIR=/usr/bin
PROG=	ftp

OBJS=	ftp.o local.o file.o other.o net.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 8kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG)

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

ftp.o:		ftp.c	ftp.h	local.h	file.h	other.h	net.h
local.o:	local.c	ftp.h	local.h
file.o:		file.c	ftp.h		file.h		net.h
other.o:	other.c	ftp.h			other.h
net.o:		net.c	ftp.h		file.h		net.h
