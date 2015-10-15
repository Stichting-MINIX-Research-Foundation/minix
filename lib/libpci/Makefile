#	$NetBSD: Makefile,v 1.5 2014/09/21 14:32:37 christos Exp $

.include <bsd.own.mk>

LIB=	pci

SRCS=	pci_bus.c pci_device.c pci_drvname.c pci_subr.c dev_verbose.c
SYSDIR= ${NETBSDSRCDIR}/sys/

.PATH.c: ${SYSDIR}/dev/pci ${SYSDIR}/dev
CPPFLAGS+= -I${SYSDIR}

MAN=	pci.3

MLINKS=	pci.3 pcibus_conf_read.3 \
	pci.3 pcibus_conf_write.3 \
	pci.3 pcidev_conf_read.3 \
	pci.3 pcidev_conf_write.3 \
	pci.3 pci_findvendor.3 \
	pci.3 pci_devinfo.3 \
	pci.3 pci_conf_print.3

INCSDIR= /usr/include
INCS=	pci.h

.include <bsd.lib.mk>
