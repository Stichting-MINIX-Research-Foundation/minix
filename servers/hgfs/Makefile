# Makefile for VMware Host/Guest File System (HGFS) server
PROG=	hgfs
SRCS=	hgfs.c

DPADD+=	${LIBSFFS} ${LIBHGFS} ${LIBSYS}
LDADD+=	-lsffs -lhgfs -lsys

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
