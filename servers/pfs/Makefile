# Makefile for Pipe File System (PFS)
PROG=	pfs
SRCS=	open.c table.c inode.c main.c super.c link.c \
	buffer.c read.c misc.c mount.c utility.c stadir.c \
	uds.c dev_uds.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

LDADD+= -lc

MAN=

BINDIR?= /usr/sbin

.include <minix.bootprog.mk>
