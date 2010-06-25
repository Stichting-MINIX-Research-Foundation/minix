# Makefile for AMD's DEV
PROG=	amddev
SRCS=	amddev.c

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
