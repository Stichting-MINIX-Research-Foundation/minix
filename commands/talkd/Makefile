# Makefile for talkd
#
# 07/22/96 			Michael Temari, <temari@ix.netcom.com>
#

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i
BINDIR=	/usr/bin
PROG=	talkd

OBJS=	talkd.o net.o process.o finduser.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 8kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG)

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

talkd.o:	talkd.c		talk.h	talkd.h	process.h net.h
net.o:		net.c		talk.h	talkd.h	net.h
process.o:	process.c	talk.h	talkd.h	finduser.h process.h
finduser.o:	finduser.c	talk.h		finduser.h
