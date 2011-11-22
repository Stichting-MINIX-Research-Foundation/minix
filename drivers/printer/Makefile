# Makefile for Centronics printer driver (PRINTER)
PROG=	printer
SRCS=	printer.c liveupdate.c

DPADD+= ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
