# Makefile for memory driver (MEMORY)
.include <bsd.own.mk>

PROG=	memory
SRCS=	memory.c imgrd.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 8k

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}
CPPFLAGS.imgrd.c+=	-I${.CURDIR}/../ramdisk -T /usr/tmp

imgrd.d: touch-genfiles

touch-genfiles:
	[ -e ${.CURDIR}/../ramdisk/image.c ] || touch -t 197001010000.00 ${.CURDIR}/../ramdisk/image.c

.include <bsd.prog.mk>
