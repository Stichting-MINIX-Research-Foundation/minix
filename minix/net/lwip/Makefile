# Makefile for lwip server.
.include <bsd.own.mk>

PROG=	lwip
SRCS=	lwip.c		\
	driver.c	\
	udp.c		\
	tcp.c		\
	raw_ip.c	\
	inet_config.c	\
	eth.c

.PATH:	${.CURDIR}/generic

DPADD+=	${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS} ${LIBLWIP} ${LIBNETSOCK}
LDADD+=	-lchardriver -lsys -ltimers -llwip -lnetsock

WARNS?=2

CPPFLAGS+= -I${NETBSDSRCDIR}/minix/lib/liblwip/include
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/lib/liblwip/include/ipv4
CPPFLAGS+= -I${NETBSDSRCDIR}/minix/lib/liblwip/include/ipv6
CPPFLAGS+= -D_SYSTEM=1

CFLAGS+= -std=c99

.include <minix.service.mk>
