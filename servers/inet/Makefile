# Makefile for Network Server (INET).
SERVER = inet

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
n = $i/net
g = generic

# programs, flags, and libraries
CC =		cc
CPPFLAGS =	-I. -I..
CFLAGS =	$(OPT) $(CPPFLAGS) -m
LDFLAGS =	-i
LIBS =		-lsys -lutils

OBJ = 	buf.o clock.o inet.o inet_config.o \
	mnx_eth.o mq.o sr.o stacktrace.o \
	$g/udp.o $g/arp.o $g/eth.o $g/event.o \
	$g/icmp.o $g/io.o $g/ip.o $g/ip_ioctl.o \
	$g/ip_lib.o $g/ip_read.o $g/ip_write.o \
	$g/ipr.o $g/tcp.o $g/tcp_lib.o \
	$g/tcp_recv.o $g/tcp_send.o $g/ip_eth.o \
	$g/ip_ps.o $g/psip.o

# build local binary
all build:	$(SERVER)
$(SERVER):	inet.a
	$(CC) -o $@ $(LDFLAGS) inet.a version.c $(LIBS)
	install -S 2kw $@

inet.a:	$(OBJ)
	@rm -f $@
	aal cr $@ $(OBJ)

# install with other servers
install:	/usr/sbin/servers/$(SERVER)
/usr/sbin/servers/$(SERVER):	$(SERVER)
	install -o root -cs $? $@

clean:
	rm -f $(SERVER) *.a *.o */*.o */*.a *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c generic/*.c > .depend

# Include generated dependencies.
include .depend

$g/arp.o:	$g/arp.c
	cd generic && $(CC) -c $(CFLAGS) arp.c
$g/eth.o:	$g/eth.c
	cd generic && $(CC) -c $(CFLAGS) eth.c
$g/event.o:	$g/event.c
	cd generic && $(CC) -c $(CFLAGS) event.c
$g/icmp.o:	$g/icmp.c
	cd generic && $(CC) -c $(CFLAGS) icmp.c
$g/io.o:	$g/io.c
	cd generic && $(CC) -c $(CFLAGS) io.c
$g/ip.o:	$g/ip.c
	cd generic && $(CC) -c $(CFLAGS) ip.c
$g/ip_eth.o:	$g/ip_eth.c
	cd generic && $(CC) -c $(CFLAGS) ip_eth.c
$g/ip_ioctl.o:	$g/ip_ioctl.c
	cd generic && $(CC) -c $(CFLAGS) ip_ioctl.c
$g/ip_lib.o:	$g/ip_lib.c
	cd generic && $(CC) -c $(CFLAGS) ip_lib.c
$g/ip_ps.o:	$g/ip_ps.c
	cd generic && $(CC) -c $(CFLAGS) ip_ps.c
$g/ip_read.o:	$g/ip_read.c
	cd generic && $(CC) -c $(CFLAGS) ip_read.c
$g/ip_write.o:	$g/ip_write.c
	cd generic && $(CC) -c $(CFLAGS) ip_write.c
$g/ipr.o:	$g/ipr.c
	cd generic && $(CC) -c $(CFLAGS) ipr.c
$g/psip.o:	$g/psip.c
	cd generic && $(CC) -c $(CFLAGS) psip.c
$g/tcp.o:	$g/tcp.c
	cd generic && $(CC) -c $(CFLAGS) tcp.c
$g/tcp_lib.o:	$g/tcp_lib.c
	cd generic && $(CC) -c $(CFLAGS) tcp_lib.c
$g/tcp_recv.o:	$g/tcp_recv.c
	cd generic && $(CC) -c $(CFLAGS) tcp_recv.c
$g/tcp_send.o:	$g/tcp_send.c
	cd generic && $(CC) -c $(CFLAGS) tcp_send.c
$g/udp.o:	$g/udp.c
	cd generic && $(CC) -c $(CFLAGS) udp.c

