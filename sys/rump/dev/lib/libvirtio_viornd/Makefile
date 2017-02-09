#	$NetBSD: Makefile,v 1.1 2014/11/06 00:45:58 pooka Exp $
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev/pci

LIB=	rumpdev_virtio_viornd
IOCONF=	VIRTIO_VIORND.ioconf

SRCS=	viornd_at_virtio.c

SRCS+=	viornd.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
