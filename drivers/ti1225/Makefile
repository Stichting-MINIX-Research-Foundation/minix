# Makefile for the Texas Instruments PCI1225 PC Card controller driver (ti1225)
PROG=	ti1225
SRCS=	ti1225.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <minix.prog.mk>
