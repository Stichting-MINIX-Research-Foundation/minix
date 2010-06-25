# Makefile for the Attansic/Atheros L2 ethernet driver (ATL2)
PROG=	atl2
SRCS=	atl2.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
