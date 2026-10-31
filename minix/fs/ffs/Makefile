# Makefile for the FFS (UFS2) filesystem server
PROG=	ffs
SRCS=	main.c table.c utility.c misc.c super.c mount.c inode.c \
	read.c path.c stadir.c balloc.c ialloc.c write.c open.c link.c \
	protect.c time.c
DPADD+=	${LIBMINIXFS} ${LIBFSDRIVER} ${LIBBDEV} ${LIBSYS}
LDADD+=	-lminixfs -lfsdriver -lbdev -lsys

WARNS=3

.include <minix.service.mk>
