# Makefile for Data Store Server (DS)
PROG=	ds
SRCS=	main.c store.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 16k

.include <bsd.prog.mk>
