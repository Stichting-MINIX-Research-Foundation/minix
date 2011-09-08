# Makefile for ext2 filesystem
PROG=	ext2
SRCS=	balloc.c cache.c device.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c table.c time.c utility.c \
	write.c ialloc.c inode.c main.c path.c \
	super.c optset.c
DPADD+=	${LIBSYS}
LDADD+= -lminixfs -lsys

MAN=

BINDIR?= /sbin
INSTALLFLAGS+= -S 128k

.include <minix.service.mk>
