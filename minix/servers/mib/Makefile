# Makefile for the Management Information Base (MIB) server

PROG=	mib
SRCS=	main.c tree.c kern.c minix.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

WARNS?=	5

.include <minix.service.mk>
