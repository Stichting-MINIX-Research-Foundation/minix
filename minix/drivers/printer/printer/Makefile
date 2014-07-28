# Makefile for Centronics printer driver (PRINTER)
PROG=	printer
SRCS=	printer.c liveupdate.c

DPADD+= ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

.include <minix.service.mk>
