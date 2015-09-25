# Makefile for VirtualBox Shared Folders File System (VBFS) server
PROG=	vbfs
SRCS=	vbfs.c
MAN=	vbfs.8

DPADD+=	${LIBSFFS} ${LIBVBOXFS} ${LIBFSDRIVER} ${LIBSYS}
LDADD+=	-lsffs -lvboxfs -lfsdriver -lsys

.include <minix.service.mk>
