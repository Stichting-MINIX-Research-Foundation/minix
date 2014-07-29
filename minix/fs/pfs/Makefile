# Makefile for Pipe File System (PFS)
PROG=	pfs
SRCS=	open.c table.c inode.c main.c super.c link.c \
	buffer.c read.c misc.c mount.c utility.c stadir.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

LDADD+= -lc

.include <minix.service.mk>
