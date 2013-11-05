# Makefile for the VNode Disk driver (VND)
PROG=	vnd
SRCS=	vnd.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
