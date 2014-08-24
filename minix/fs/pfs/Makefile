# Makefile for Pipe File System (PFS)
PROG=	pfs
SRCS=	pfs.c

DPADD+=	${LIBFSDRIVER} ${LIBSYS}
LDADD+=	-lfsdriver -lsys

.include <minix.service.mk>
