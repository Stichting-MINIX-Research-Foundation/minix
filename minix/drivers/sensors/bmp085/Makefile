# Makefile for the bmp085 pressure and temp sensor found on the Weather Cape.
PROG=	bmp085
SRCS=	bmp085.c

DPADD+=	${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lchardriver -lsys -ltimers

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
