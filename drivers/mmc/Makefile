# Makefile for the hello driver.
PROG=	mmcblk
SRCS=	mmcblk.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys
CLEANFILES+=.depend mmcblk.d

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
