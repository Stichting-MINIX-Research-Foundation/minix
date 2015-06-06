# Makefile for ISO9660 fs
PROG=	isofs
SRCS=	main.c table.c mount.c super.c inode.c \
	link.c utility.c path.c read.c susp.c susp_rock_ridge.c stadir.c

DPADD+=	${LIBFSDRIVER} ${LIBBDEV} ${LIBSYS} ${LIBMINIXFS}
LDADD+=	-lfsdriver -lbdev -lsys -lminixfs

CPPFLAGS+= -DNR_BUFS=100

.include <minix.service.mk>
