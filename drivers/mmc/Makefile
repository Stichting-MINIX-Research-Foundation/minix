# Makefile for the hello driver.
PROG=	mmcblk
SRCS=	mmcblk.c mmclog.h

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CLEANFILES+=.depend mmcblk.d

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1
CFLAGS+= -g
MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
