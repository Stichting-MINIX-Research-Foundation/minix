# Makefile for the UNIX Domain Sockets driver (UDS)
PROG=	uds
SRCS=	uds.c io.c stat.c
MAN=	unix.8

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBSOCKEVENT} ${LIBSOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsockevent -lsockdriver -lsys -ltimers

WARNS?=	5

.include <minix.service.mk>
