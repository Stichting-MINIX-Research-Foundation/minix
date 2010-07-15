# Makefile for Pipe File System (PFS)
PROG=	pfs
SRCS=	open.c table.c inode.c main.c super.c link.c \
	buffer.c read.c misc.c utility.c stadir.c \
	uds.c dev_uds.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 128k

.include <bsd.prog.mk>
