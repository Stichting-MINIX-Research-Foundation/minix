# Makefile for PCI support
PROG=	pci
SRCS=	main.c pci.c pci_table.c

DPADD+=	${LIBDRIVER} ${LIBSYS} ${LIBTIMERS}
LDADD+=	-ldriver -lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <bsd.prog.mk>
