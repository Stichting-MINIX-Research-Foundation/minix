# Makefile for the i2c eeprom commonly found on the BeagleBone (CAT24C256)
PROG=	cat24c256
SRCS=	cat24c256.c

DPADD+=	${LIBI2CDRIVER} ${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lblockdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
