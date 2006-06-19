# Makefile for ftp
#
# 01/25/96 Initial Release	Michael Temari
# 03/08/00			Michael Temari, <Michael@TemWare.Com>
# 02/07/05 v. 1.01		Michael Temari, <Michael@TemWare.Com>

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE -m
LDFLAGS=-i
BINDIR=/usr/bin
PROG=	ftp
MANPAGE= /usr/local/man/man1

OBJS=	ftp.o local.o file.o xfer.o other.o net.o crc.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 8kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG)

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

installman:	$(MANPAGE)

	cp ftp.1 /usr/local/man/man1
	makewhatis /usr/local/man

crc.o:		crc.c
	$(CC) -c $(CFLAGS) -DCRC_ONLY $?

ftp.o:		ftp.c	ftp.h	local.h	file.h	other.h	net.h
local.o:	local.c	ftp.h	local.h
file.o:		file.c	ftp.h		file.h		net.h
other.o:	other.c	ftp.h			other.h
net.o:		net.c	ftp.h			xfer.h	net.h
xfer.o:		xfer.c	ftp.h			xfer.h
