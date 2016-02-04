# Makefile for the gpio driver.
PROG=	gpio
SRCS=	gpio.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

DPADD+=	${LIBVTREEFS} ${LIBFSDRIVER} ${LIBSYS} ${LIBGPIO} ${LIBCLKCONF}
LDADD+=	-lvtreefs -lfsdriver -lsys -lgpio -lclkconf

# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>
