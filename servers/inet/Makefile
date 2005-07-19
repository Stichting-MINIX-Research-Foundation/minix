# Makefile for inet.

# Directories
g = generic

# Programs, flags, and libraries
CC =		cc
CPPFLAGS =	-I. -D_MINIX
CFLAGS =	$(OPT) $(CPPFLAGS)
LDFLAGS =
LIBS =		-lsys -lsysutil

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

OBJ = 	buf.o clock.o inet.o inet_config.o \
	mnx_eth.o mq.o qp.o sr.o stacktrace.o \
	$g/udp.o $g/arp.o $g/eth.o $g/event.o \
	$g/icmp.o $g/io.o $g/ip.o $g/ip_ioctl.o \
	$g/ip_lib.o $g/ip_read.o $g/ip_write.o \
	$g/ipr.o $g/rand256.o $g/tcp.o $g/tcp_lib.o \
	$g/tcp_recv.o $g/tcp_send.o $g/ip_eth.o \
	$g/ip_ps.o $g/psip.o \
	minix3/queryparam.o sha2.o

all:	inet

inet:	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) version.c $(LIBS)

install:	/usr/sbin/inet

/usr/sbin/inet:	inet
	install -c $? $@

clean:
	rm -f $(OBJ) inet *.bak

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c generic/*.c > .depend

# Include generated dependencies.
include .depend

#
# $PchId: Makefile.mnx3,v 1.1 2005/06/28 14:28:45 philip Exp $
#
