# Makefile for ISO9660 fs
PROG=	isofs
SRCS=	main.c table.c mount.c super.c inode.c \
	utility.c misc.c path.c read.c stadir.c

DPADD+=	${LIBBDEV} ${LIBSYS}
LDADD+=	-lbdev -lsys -lc -lminixfs

MAN=

BINDIR?= /sbin

NR_BUFS= 100
CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

.include <minix.service.mk>
