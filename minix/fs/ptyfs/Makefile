# Makefile for PTYFS server
.include <bsd.own.mk>

PROG=	ptyfs
SRCS=	ptyfs.c node.c

DPADD+=	${LIBFSDRIVER}
LDADD+=	-lfsdriver

.include <minix.service.mk>
