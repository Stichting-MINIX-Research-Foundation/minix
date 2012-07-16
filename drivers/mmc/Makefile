# Makefile for the hello driver.
PROG=	mmcblk
SRCS=	mmcblk.c mmclog.h

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CLEANFILES+=.depend mmcblk.d

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
