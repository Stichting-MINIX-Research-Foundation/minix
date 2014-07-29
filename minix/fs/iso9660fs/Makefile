# Makefile for ISO9660 fs
PROG=	isofs
SRCS=	main.c table.c mount.c super.c inode.c \
	utility.c misc.c path.c read.c stadir.c

DPADD+=	${LIBBDEV} ${LIBSYS} ${LIBMINLIB}
LDADD+=	-lbdev -lsys -lc -lminixfs -lminlib

CPPFLAGS+= -DNR_BUFS=100

.include <minix.service.mk>
