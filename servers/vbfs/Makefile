# Makefile for VirtualBox Shared Folders File System (VBFS) server
PROG=	vbfs
SRCS=	vbfs.c

DPADD+=	${LIBSFFS} ${LIBVBOXFS} ${LIBSYS}
LDADD+=	-lsffs -lvboxfs -lsys

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
