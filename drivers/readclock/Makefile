# Makefile for readclock 'driver'
PROG=	readclock.drv
SRCS=	readclock.c

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /sbin

CPPFLAGS+= -D_SYSTEM=1

.include <bsd.prog.mk>
