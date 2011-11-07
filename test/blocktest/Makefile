# Makefile for the Block Device Driver Test driver (blocktest)
PROG=	blocktest
SRCS=	blocktest.c optset.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys
CPPFLAGS+=-I${MINIXSRCDIR}

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
