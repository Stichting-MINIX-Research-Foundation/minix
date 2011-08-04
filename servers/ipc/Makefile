# Makefile for IPC server
PROG=	ipc
SRCS=	main.c utility.c shm.c sem.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /usr/sbin
FILES=ipc.conf
FILESNAME=ipc
FILESDIR= /etc/system.conf.d

.include <minix.service.mk>
