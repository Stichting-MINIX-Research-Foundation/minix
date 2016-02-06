# Makefile for Centronics printer driver (PRINTER)
PROG=	printer
SRCS=	printer.c liveupdate.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

.include <minix.service.mk>
