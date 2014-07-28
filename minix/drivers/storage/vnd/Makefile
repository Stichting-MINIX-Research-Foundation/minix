# Makefile for the VNode Disk driver (VND)
PROG=	vnd
SRCS=	vnd.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys

.include <minix.service.mk>
