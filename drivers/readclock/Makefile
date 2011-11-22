# Makefile for readclock 'driver'
PROG=	readclock.drv
SRCS=	readclock.c

DPADD+= ${LIBSYS}
LDADD+=	-lsys

MAN=

BINDIR?= /sbin

CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>
