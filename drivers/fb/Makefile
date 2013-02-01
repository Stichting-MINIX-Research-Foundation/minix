# Makefile for the framebuffer driver.
PROG=	fb

.include "arch/${MACHINE_ARCH}/Makefile.inc"

SRCS+=	fb.c

DPADD+=	${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
