# Makefile for VirtualBox Shared Folders File System (VBFS) server
PROG=	vbfs
SRCS=	vbfs.c

DPADD+=	${LIBSFFS} ${LIBVBOXFS} ${LIBSYS}
LDADD+=	-lsffs -lvboxfs -lsys

.include <minix.service.mk>
