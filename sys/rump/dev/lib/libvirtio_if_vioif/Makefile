#	$NetBSD: Makefile,v 1.1 2014/08/22 09:48:54 pooka Exp $
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci

LIB=	rumpdev_virtio_if_vioif
IOCONF=	VIRTIO_IF_VIOIF.ioconf

SRCS=	vioif_at_virtio.c

SRCS+=	if_vioif.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
