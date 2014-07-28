# Makefile for the Faulty Block Device (FBD)

.include <bsd.own.mk>

PROG=	fbd
SRCS=	fbd.c rule.c action.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CPPFLAGS+=	-DDEBUG=0

# The FBD driver requires NetBSD libc.

.include <minix.service.mk>
