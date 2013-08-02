# Makefile for readclock 'driver'
PROG=	readclock.drv

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS+=	readclock.c

DPADD+= ${LIBSYS} ${LIBTIMERS}
LDADD+= -lsys -ltimers

MAN=

BINDIR?= /sbin

CPPFLAGS+= -D_SYSTEM=1 -I${.CURDIR}

.include <minix.service.mk>
