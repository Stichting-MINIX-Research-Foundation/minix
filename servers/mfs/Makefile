# Makefile for Minix File System (MFS)
PROG=	mfs
SRCS=	cache.c device.c link.c \
	mount.c misc.c open.c protect.c read.c \
	stadir.c stats.c table.c time.c utility.c \
	write.c inode.c main.c path.c super.c

DPADD+=	${LIBM} ${LIBSYS}
LDADD+= -lminixfs  -lsys

MAN=

BINDIR?= /sbin
INSTALLFLAGS+=	-S 128k

DEFAULT_NR_BUFS= 1024
CPPFLAGS+= -DDEFAULT_NR_BUFS=${DEFAULT_NR_BUFS}

.include <minix.bootprog.mk>
