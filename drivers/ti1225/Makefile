# Makefile for the Texas Instruments PCI1225 PC Card controller driver (ti1225)
PROG=	ti1225
SRCS=	ti1225.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
