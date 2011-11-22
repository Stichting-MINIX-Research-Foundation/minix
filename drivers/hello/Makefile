# Makefile for the hello driver.
PROG=	hello
SRCS=	hello.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
