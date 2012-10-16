# Makefile for ext2 filesystem
PROG=	ext2
SRCS=	balloc.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c table.c time.c utility.c \
	write.c ialloc.c inode.c main.c path.c \
	super.c
DPADD+=	${LIBMINIXFS} ${LIBBDEV} ${LIBSYS}
LDADD+= -lminixfs -lbdev -lsys

MAN=

BINDIR?= /sbin
CFLAGS+= -Wall -Wextra

.include <minix.service.mk>
