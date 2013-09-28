# Makefile for the Block Device Driver Test driver (blocktest)
PROG=	blocktest
SRCS=	blocktest.c
FILES=	system.conf support.sh test.sh README

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/tests/minix-posix/blocktest
FILESDIR?= /usr/tests/minix-posix/blocktest

.include "Makefile.inc"
.include <minix.service.mk>
