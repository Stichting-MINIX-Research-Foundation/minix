# Makefile for the Remote MIB test service (rmibtest)
PROG=	rmibtest
SRCS=	rmibtest.c
FILES=	rmibtest.conf

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/tests/minix-posix/rmibtest
FILESDIR?= /usr/tests/minix-posix/rmibtest

.include <minix.service.mk>
