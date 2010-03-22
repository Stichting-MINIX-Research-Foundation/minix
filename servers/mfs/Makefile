# Makefile for Minix File System (MFS)
PROG=	mfs
SRCS=	cache.c device.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c table.c time.c utility.c \
	write.c inode.c main.c path.c super.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

NR_BUFS= 1024
CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

.include <minix.prog.mk>
