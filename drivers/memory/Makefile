# Makefile for memory driver (MEMORY)
PROG=	memory
SRCS=	memory.c imgrd.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lchardriver -lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 8k

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk 

.include <minix.bootprog.mk>
