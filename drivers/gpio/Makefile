# Makefile for the gpio driver.
PROG=	gpio
SRCS=	gpio.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBGPIO} ${LIBCLKCONF}
LDADD+=	-lvtreefs -lsys -lgpio -lclkconf

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
