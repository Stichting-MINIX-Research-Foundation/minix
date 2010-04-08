# Makefile for readclock 'driver'
PROG=	readclock.drv
SRCS=	readclock.c

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /sbin

CPPFLAGS+= -D_MINIX=1 -D_POSIX_SOURCE=1 -D_SYSTEM=1

.include <minix.prog.mk>
