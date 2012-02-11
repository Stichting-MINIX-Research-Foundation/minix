# Makefile for the Advanced Host Controller Interface driver (AHCI)
PROG=	ahci
SRCS=	ahci.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS} ${LIBTIMERS} ${LIBMTHREAD}
LDADD+=	-lblockdriver -lsys -ltimers -lmthread

LDADD+=	-lc

MAN=

BINDIR?= /sbin

.include <minix.service.mk>
