# Makefile for ISO9660 fs
PROG=	isofs
SRCS=	main.c table.c mount.c super.c inode.c device.c \
	utility.c misc.c path.c read.c stadir.c cache.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /sbin

NR_BUFS= 100
CPPFLAGS+= -DNR_BUFS=${NR_BUFS}

.include <bsd.prog.mk>
