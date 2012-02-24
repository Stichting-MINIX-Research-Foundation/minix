# Makefile for inet.
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

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=  -I${.CURDIR} -D_MINIX -D_SYSTEM
CPPFLAGS+= -I${.CURDIR}/../../lib/liblwip/include

CFLAGS += -Wextra -std=c99

.include <minix.service.mk>
