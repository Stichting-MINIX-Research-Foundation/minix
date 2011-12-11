# Makefile for the Block Device Driver Test driver (blocktest)
PROG=	blocktest
SRCS=	blocktest.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin

.include "Makefile.inc"
.include <minix.service.mk>
