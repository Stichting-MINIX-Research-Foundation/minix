# Makefile for the tda19988 HDMI framer found on the BeagleBone Black.
PROG=	tda19988
SRCS=	tda19988.c

DPADD+=	${LIBI2CDRIVER} ${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lblockdriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
