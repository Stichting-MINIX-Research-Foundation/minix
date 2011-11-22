# Makefile for the AT disk driver (AT_WINI)
PROG=	at_wini
SRCS=	at_wini.c liveupdate.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lblockdriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
