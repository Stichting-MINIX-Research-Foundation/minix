.include <bsd.own.mk>

# Makefile for PCI support
PROG=	pci
SRCS=	main.c pci.c pci_table.c

# Directly taken from sys/dev/pci
.PATH.c: ${NETBSDSRCDIR}/sys/dev/pci
SRCS+=	pci_verbose.c pci_subr.c
CPPFLAGS.pci_subr.c+= -D_PCI_SERVER

DPADD+=	${LIBCHARDRIVER}
LDADD+=	-lchardriver

WARNS=	3

.include <minix.service.mk>
