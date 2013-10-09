# Makefile for the input server

PROG=	input
SRCS=	input.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

MAN=

BINDIR?= /sbin

.include <minix.service.mk>

