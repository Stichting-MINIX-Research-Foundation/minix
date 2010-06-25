# Makefile for Lance driver
PROG=	lance
SRCS=	lance.c

DPADD+=	${LIBNETDRIVER} ${LIBSYS}
LDADD+=	-lnetdriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
