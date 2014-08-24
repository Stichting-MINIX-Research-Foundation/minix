# Makefile for Pipe File System (PFS)
PROG=	pfs
SRCS=	open.c table.c inode.c main.c super.c link.c \
	buffer.c read.c misc.c mount.c stadir.c

DPADD+=	${LIBFSDRIVER} ${LIBSYS}
LDADD+=	-lfsdriver -lsys

.include <minix.service.mk>
