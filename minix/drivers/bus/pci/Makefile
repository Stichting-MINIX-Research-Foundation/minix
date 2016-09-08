.include <bsd.own.mk>

# Makefile for PCI support
PROG=	pci
SRCS=	main.c pci.c pci_table.c

# Directly taken from sys/dev/pci
SRCS+=	dev_verbose.c pci_subr.c

.PATH.c: ${NETBSDSRCDIR}/sys/dev/pci
.PATH.c: ${NETBSDSRCDIR}/sys/dev

CPPFLAGS+= -I ${NETBSDSRCDIR}/sys
CPPFLAGS.pci_subr.c+= -D_PCI_SERVER

CWARNFLAGS.clang=-Wno-unused

DPADD+=	${LIBCHARDRIVER}
LDADD+=	-lchardriver

DPADD+=	${LIBACPI}
LDADD+=	-lacpi

WARNS=	3

.include <minix.service.mk>
