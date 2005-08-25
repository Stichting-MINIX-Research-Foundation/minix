# Makefile for lib/ip.
#
# Note: The oneC_sum.c file is not used if there is an assembly equivalent.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -I. -DNDEBUG
CC1	= $(CC) $(CFLAGS) -c
MAKE	= exec make -$(MAKEFLAGS)

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(accept.o) \
	$(LIBRARY)(bind.o) \
	$(LIBRARY)(connect.o) \
	$(LIBRARY)(dhcp_gettag.o) \
	$(LIBRARY)(dhcp_settag.o) \
	$(LIBRARY)(ether_line.o) \
	$(LIBRARY)(ethera2n.o) \
	$(LIBRARY)(ethere2a.o) \
	$(LIBRARY)(etherh2n.o) \
	$(LIBRARY)(ethern2h.o) \
	$(LIBRARY)(getdomain.o) \
	$(LIBRARY)(gethnmadr.o) \
	$(LIBRARY)(gethostent.o) \
	$(LIBRARY)(gethostname.o) \
	$(LIBRARY)(getnetent.o) \
	$(LIBRARY)(getnetbyname.o) \
	$(LIBRARY)(getnetbyaddr.o) \
	$(LIBRARY)(getpeername.o) \
	$(LIBRARY)(getproto.o) \
	$(LIBRARY)(getprotoent.o) \
	$(LIBRARY)(getservent.o) \
	$(LIBRARY)(getsockname.o) \
	$(LIBRARY)(getsockopt.o) \
	$(LIBRARY)(getsrvbyname.o) \
	$(LIBRARY)(getsrvbyport.o) \
	$(LIBRARY)(hton.o) \
	$(LIBRARY)(inet_addr.o) \
	$(LIBRARY)(inet_network.o) \
	$(LIBRARY)(inet_ntoa.o) \
	$(LIBRARY)(listen.o) \
	$(LIBRARY)(memcspn.o) \
	$(LIBRARY)(rcmd.o) \
	$(LIBRARY)(recv.o) \
	$(LIBRARY)(recvfrom.o) \
	$(LIBRARY)(res_comp.o) \
	$(LIBRARY)(res_init.o) \
	$(LIBRARY)(res_mkquery.o) \
	$(LIBRARY)(res_query.o) \
	$(LIBRARY)(res_send.o) \
	$(LIBRARY)(ruserok.o) \
	$(LIBRARY)(send.o) \
	$(LIBRARY)(sendto.o) \
	$(LIBRARY)(sethostent.o) \
	$(LIBRARY)(setsockopt.o) \
	$(LIBRARY)(servxcheck.o) \
	$(LIBRARY)(shutdown.o) \
	$(LIBRARY)(socket.o) \
	$(LIBRARY)(strcasecmp.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(accept.o):	accept.c
	$(CC1) accept.c

$(LIBRARY)(bind.o):	bind.c
	$(CC1) bind.c

$(LIBRARY)(connect.o):	connect.c
	$(CC1) connect.c

$(LIBRARY)(dhcp_gettag.o):	dhcp_gettag.c
	$(CC1) dhcp_gettag.c

$(LIBRARY)(dhcp_settag.o):	dhcp_settag.c
	$(CC1) dhcp_settag.c

$(LIBRARY)(ether_line.o):	ether_line.c
	$(CC1) ether_line.c

$(LIBRARY)(ethera2n.o):	ethera2n.c
	$(CC1) ethera2n.c

$(LIBRARY)(ethere2a.o):	ethere2a.c
	$(CC1) ethere2a.c

$(LIBRARY)(etherh2n.o):	etherh2n.c
	$(CC1) etherh2n.c

$(LIBRARY)(ethern2h.o):	ethern2h.c
	$(CC1) ethern2h.c

$(LIBRARY)(getdomain.o):	getdomain.c
	$(CC1) getdomain.c

$(LIBRARY)(gethnmadr.o):	gethnmadr.c
	$(CC1) gethnmadr.c

$(LIBRARY)(gethostent.o):	gethostent.c
	$(CC1) gethostent.c

$(LIBRARY)(gethostname.o):	gethostname.c
	$(CC1) gethostname.c

$(LIBRARY)(getnetent.o):	getnetent.c
	$(CC1) getnetent.c

$(LIBRARY)(getnetbyname.o):	getnetbyname.c
	$(CC1) getnetbyname.c

$(LIBRARY)(getnetbyaddr.o):	getnetbyaddr.c
	$(CC1) getnetbyaddr.c

$(LIBRARY)(getpeername.o):	getpeername.c
	$(CC1) getpeername.c

$(LIBRARY)(getproto.o):	getproto.c
	$(CC1) getproto.c

$(LIBRARY)(getprotoent.o):	getprotoent.c
	$(CC1) getprotoent.c

$(LIBRARY)(getservent.o):	getservent.c
	$(CC1) getservent.c

$(LIBRARY)(getsockname.o):	getsockname.c
	$(CC1) getsockname.c

$(LIBRARY)(getsockopt.o):	getsockopt.c
	$(CC1) getsockopt.c

$(LIBRARY)(getsrvbyname.o):	getsrvbyname.c
	$(CC1) getsrvbyname.c

$(LIBRARY)(getsrvbyport.o):	getsrvbyport.c
	$(CC1) getsrvbyport.c

$(LIBRARY)(hton.o):	hton.c
	$(CC1) hton.c

$(LIBRARY)(inet_addr.o):	inet_addr.c
	$(CC1) inet_addr.c

$(LIBRARY)(inet_network.o):	inet_network.c
	$(CC1) inet_network.c

$(LIBRARY)(inet_ntoa.o):	inet_ntoa.c
	$(CC1) inet_ntoa.c

$(LIBRARY)(listen.o):	listen.c
	$(CC1) listen.c

$(LIBRARY)(memcspn.o):	memcspn.c
	$(CC1) memcspn.c

$(LIBRARY)(oneC_sum.o):	oneC_sum.c
	$(CC1) oneC_sum.c

$(LIBRARY)(rcmd.o):	rcmd.c
	$(CC1) rcmd.c

$(LIBRARY)(recv.o):	recv.c
	$(CC1) recv.c

$(LIBRARY)(recvfrom.o):	recvfrom.c
	$(CC1) recvfrom.c

$(LIBRARY)(res_comp.o):	res_comp.c
	$(CC1) res_comp.c

$(LIBRARY)(res_init.o):	res_init.c
	$(CC1) res_init.c

$(LIBRARY)(res_mkquery.o):	res_mkquery.c
	$(CC1) res_mkquery.c

$(LIBRARY)(res_query.o):	res_query.c
	$(CC1) res_query.c

$(LIBRARY)(res_send.o):	res_send.c
	$(CC1) res_send.c

$(LIBRARY)(ruserok.o):	ruserok.c
	$(CC1) ruserok.c

$(LIBRARY)(send.o):	send.c
	$(CC1) send.c

$(LIBRARY)(sendto.o):	sendto.c
	$(CC1) sendto.c

$(LIBRARY)(sethostent.o):	sethostent.c
	$(CC1) sethostent.c

$(LIBRARY)(setsockopt.o):	setsockopt.c
	$(CC1) setsockopt.c

$(LIBRARY)(servxcheck.o):	servxcheck.c
	$(CC1) servxcheck.c

$(LIBRARY)(shutdown.o):	shutdown.c
	$(CC1) shutdown.c

$(LIBRARY)(socket.o):	socket.c
	$(CC1) socket.c

$(LIBRARY)(strcasecmp.o):	strcasecmp.c
	$(CC1) strcasecmp.c
