# Makefile for the tps65217 Power Management IC found on the BeagleBones
PROG=	tps65217
SRCS=	tps65217.c

DPADD+= ${LIBI2CDRIVER} ${CLKCONF} ${LIBSYS} ${LIBTIMERS}
LDADD+= -li2cdriver -lclkconf -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=      -I${NETBSDSRCDIR}

.include <minix.service.mk>
