# Makefile for the BIOS disk driver (BIOS_WINI)
PROG=	bios_wini
SRCS=	bios_wini.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-lblockdriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
