# Makefile for urlget
#
# 07/02/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
#

CFLAGS=	-D_MINIX $(OPT)
LDFLAGS=-fnone
BINDIR=	/usr/bin
PROG=	urlget
LINK1=	httpget
LINK2=	ftpget

OBJS=	urlget.o net.o

all:	$(PROG)

$(PROG):	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	install -S 4kw $@

clean:
	rm -f $(PROG) $(OBJS)

tar:
	tar cvf urlget.tar Makefile urlget.c net.c net.h

install:	$(BINDIR)/$(PROG) $(BINDIR)/$(LINK1) $(BINDIR)/$(LINK2)

$(BINDIR)/$(PROG):	$(PROG)
	install -c $? $@

$(BINDIR)/$(LINK1) $(BINDIR)/$(LINK2):	$(BINDIR)/$(PROG)
	install -l $? $@

$(OBJS):	net.h
