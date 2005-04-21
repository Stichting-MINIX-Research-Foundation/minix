# Makefile for telnetd
#
# 01/30/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
#

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i
BINDIR=	/usr/bin
PROG=	in.telnetd

OBJS=	main.o telnet.o term.o pty.o wtmp.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 8kw $@

clean:
	rm -f $(PROG) $(OBJS)

install:	$(BINDIR)/$(PROG)

$(BINDIR)/$(PROG):	$(PROG)
	install -cs -o bin $? $@

main.o:		main.c		telnetd.h
telnet.o:	telnet.c	telnetd.h	telnet.h
term.o:		term.c		telnetd.h	telnet.h
pty.o:		pty.c		telnetd.h
wtmp.o:				telnetd.h
