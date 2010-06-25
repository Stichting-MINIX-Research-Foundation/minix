# Makefile for inet.
PROG=	inet
SRCS=	buf.c clock.c inet.c inet_config.c \
	mnx_eth.c mq.c qp.c sr.c \
	udp.c arp.c eth.c event.c \
	icmp.c io.c ip.c ip_ioctl.c \
	ip_lib.c ip_read.c ip_write.c \
	ipr.c rand256.c tcp.c tcp_lib.c \
	tcp_recv.c tcp_send.c ip_eth.c \
	ip_ps.c psip.c \
	queryparam.c sha2.c version.c

.PATH:	${.CURDIR}/generic

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=  -I${.CURDIR} -D_MINIX

.include <bsd.prog.mk>
