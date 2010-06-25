# Makefile for Centronics printer driver (PRINTER)
PROG=	printer
SRCS=	printer.c liveupdate.c

DPADD+= ${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
