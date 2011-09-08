# Makefile for inet.
PROG=	lwip
SRCS=	lwip.c		\
	socket.c	\
	driver.c	\
	udp.c		\
	tcp.c		\
	raw_ip.c	\
	inet_config.c	\
	eth.c

.PATH:	${.CURDIR}/generic

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBMINIXUTIL} ${LIBLWIP}
LDADD+=	-ldriver -lsys -lminixutil -ltimers -llwip

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=  -I${.CURDIR} -D_MINIX -D_SYSTEM
CPPFLAGS+= -I${.CURDIR}/../../lib/liblwip/include

CFLAGS += -Wextra -std=c99

.include <minix.service.mk>
