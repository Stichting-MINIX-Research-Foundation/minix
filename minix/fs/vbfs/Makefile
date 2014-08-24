# Makefile for VirtualBox Shared Folders File System (VBFS) server
PROG=	vbfs
SRCS=	vbfs.c

DPADD+=	${LIBSFFS} ${LIBVBOXFS} ${LIBFSDRIVER} ${LIBSYS}
LDADD+=	-lsffs -lvboxfs -lfsdriver -lsys

.include <minix.service.mk>
