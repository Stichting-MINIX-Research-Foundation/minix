# Makefile for VMware Host/Guest File System (HGFS) server
PROG=	hgfs
SRCS=	dentry.c handle.c inode.c link.c lookup.c main.c \
	misc.c mount.c name.c optset.c path.c read.c \
	stat.c table.c util.c verify.c write.c

DPADD+=	${LIBHGFS} ${LIBSYS}
LDADD+=	-lhgfs -lsys

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
