# Makefile for VMware Host/Guest File System (HGFS) server
PROG=	hgfs
SRCS=	hgfs.c
MAN=	hgfs.8

DPADD+=	${LIBSFFS} ${LIBHGFS} ${LIBFSDRIVER} ${LIBSYS}
LDADD+=	-lsffs -lhgfs -lfsdriver -lsys

.include <minix.service.mk>
