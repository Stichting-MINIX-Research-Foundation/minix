# Makefile for the sht21 humidity and temp sensor found on the Weather Cape.

PROG=   sht21
SRCS=   sht21.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBI2CDRIVER} ${LIBCHARDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-li2cdriver -lchardriver -lsys -ltimers

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.service.mk>
