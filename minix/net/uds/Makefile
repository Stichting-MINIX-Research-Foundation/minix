# Makefile for the UNIX Domain Sockets driver (UDS)
PROG=	uds
SRCS=	uds.c ioc_uds.c
MAN=	uds.8 unix.8

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

.include <minix.service.mk>
