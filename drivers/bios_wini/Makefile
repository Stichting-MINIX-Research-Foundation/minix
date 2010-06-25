# Makefile for the BIOS disk driver (BIOS_WINI)
PROG=	bios_wini
SRCS=	bios_wini.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
