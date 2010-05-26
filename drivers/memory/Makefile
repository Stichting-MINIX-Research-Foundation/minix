# Makefile for memory driver (MEMORY)
.include <minix.own.mk>

PROG=	memory
SRCS=	memory.c imgrd.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 8k

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk -T /usr/tmp

.include <minix.prog.mk>
