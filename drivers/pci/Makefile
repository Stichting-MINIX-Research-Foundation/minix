# Makefile for PCI support
PROG=	pci
SRCS=	main.c pci.c pci_table.c

DPADD+=	${LIBSYS} ${LIBTIMERS}
LDADD+=	-lsys -ltimers

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
