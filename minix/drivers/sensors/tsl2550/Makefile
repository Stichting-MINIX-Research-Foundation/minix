# Makefile for the tsl2550 ambient light sensor found on the Weather Cape.
PROG=	tsl2550
SRCS=	tsl2550.c

DPADD+=	${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lchardriver -lsys -ltimers

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
