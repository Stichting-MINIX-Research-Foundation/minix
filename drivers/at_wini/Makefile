# Makefile for the AT disk driver (AT_WINI)
PROG=	at_wini
SRCS=	at_wini.c liveupdate.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
