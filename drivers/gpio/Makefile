# Makefile for the gpio driver.
PROG=	gpio
SRCS=	gpio.c gpio_omap.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lvtreefs -lsys

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
