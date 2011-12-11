# Makefile for the Faulty Block Device (FBD)

.include <bsd.own.mk>

PROG=	fbd
SRCS=	fbd.c rule.c action.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys -lc
CPPFLAGS+=	-DDEBUG=0

# The FBD driver requires NetBSD libc.
.if ${COMPILER_TYPE} != "gnu"
CC:=clang
COMPILER_TYPE:=gnu
.endif

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
