# Makefile for talk
#
# 08/01/96 			Michael Temari, <temari@ix.netcom.com>
#

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i
BINDIR=	/usr/bin
PROG=	talk

OBJS=	talk.o screen.o net.o proto.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lcurses
	install -S 16kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG)

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

talk.o:		talk.c	talk.h	proto.h	net.h	screen.h
screen.o:	screen.c			screen.h
net.o:		net.c	talk.h		net.h
proto.o:	proto.c	talk.h	proto.h	net.h	screen.h
