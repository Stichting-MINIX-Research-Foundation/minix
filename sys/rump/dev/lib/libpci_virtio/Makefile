#	$NetBSD: Makefile,v 1.1 2014/08/22 09:45:29 pooka Exp $
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci

LIB=	rumpdev_pci_virtio
IOCONF=	PCI_VIRTIO.ioconf

SRCS=	virtio_at_pci.c

SRCS+=	virtio.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
