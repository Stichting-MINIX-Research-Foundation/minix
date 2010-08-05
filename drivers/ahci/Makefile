# Makefile for the Advanced Host Controller Interface driver (AHCI)
PROG=	ahci
SRCS=	ahci.c

DPADD+=	${LIBDRIVER} ${LIBSYS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /sbin

.include <bsd.prog.mk>
