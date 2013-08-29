# Makefile for the sys_padconf test.

.include <bsd.own.mk>

PROG=	padconftest
SRCS=	padconftest.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/tests/minix-posix

.include "${NETBSDSRCDIR}/drivers/Makefile.inc"
.include <minix.service.mk>
