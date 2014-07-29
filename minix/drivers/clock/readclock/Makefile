# Makefile for readclock 'driver'
PROG=	readclock.drv
SRCS+=	readclock.c forward.c forward.h

.include "arch/${MACHINE_ARCH}/Makefile.inc"

DPADD+= ${LIBSYS} ${LIBTIMERS}
LDADD+= -lsys -ltimers

CPPFLAGS+= -D_SYSTEM=1 -I${.CURDIR}

.include <minix.service.mk>
