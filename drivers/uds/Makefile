# Makefile for the UNIX Domain Sockets driver (UDS)
PROG=	uds
SRCS=	uds.c ioc_uds.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
