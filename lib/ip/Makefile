# Makefile for lib/ip.
#
# Note: The oneC_sum.c file is not used if there is an assembly equivalent.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -I. -DNDEBUG

LIBRARIES = libc

libc_OBJECTS	= \
	accept.o \
	bind.o \
	connect.o \
	dhcp_gettag.o \
	dhcp_settag.o \
	ether_line.o \
	ethera2n.o \
	ethere2a.o \
	etherh2n.o \
	ethern2h.o \
	getdomain.o \
	gethnmadr.o \
	gethostent.o \
	gethostname.o \
	getnetent.o \
	getnetbyname.o \
	getnetbyaddr.o \
	getpeername.o \
	getproto.o \
	getprotoent.o \
	getservent.o \
	getsockname.o \
	getsockopt.o \
	getsrvbyname.o \
	getsrvbyport.o \
	hton.o \
	inet_addr.o \
	inet_network.o \
	inet_ntoa.o \
	listen.o \
	memcspn.o \
	rcmd.o \
	recv.o \
	recvfrom.o \
	res_comp.o \
	res_init.o \
	res_mkquery.o \
	res_query.o \
	res_send.o \
	ruserok.o \
	send.o \
	sendto.o \
	sethostent.o \
	setsockopt.o \
	servxcheck.o \
	shutdown.o \
	socket.o \
	strcasecmp.o \

include ../Makefile.inc
