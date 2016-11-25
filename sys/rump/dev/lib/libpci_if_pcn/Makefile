#	$NetBSD: Makefile,v 1.1 2014/08/13 15:01:32 pooka Exp $
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci

LIB=	rumpdev_pci_if_pcn
IOCONF=	PCI_IF_PCN.ioconf

SRCS=	pcn_at_pci.c

SRCS+=	if_pcn.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
