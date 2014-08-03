# Makefile for ISO9660 fs
PROG=	isofs
SRCS=	main.c table.c mount.c super.c inode.c \
	link.c utility.c misc.c path.c read.c susp.c susp_rock_ridge.c stadir.c

DPADD+=	${LIBBDEV} ${LIBSYS} 
LDADD+=	-lbdev -lsys -lc -lminixfs 

CPPFLAGS+= -DNR_BUFS=100 -Wall

.include <minix.service.mk>
