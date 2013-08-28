# Makefile for the gpio driver.
PROG=	gpio
SRCS=	gpio.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lvtreefs -lsys -lgpio -lclkconf

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
