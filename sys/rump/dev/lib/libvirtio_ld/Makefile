#	$NetBSD: Makefile,v 1.1 2014/08/22 09:57:05 pooka Exp $
#

RUMPTOP=${TOPRUMP}

.PATH:	${RUMPTOP}/../dev	\
	${RUMPTOP}/../dev/pci

LIB=	rumpdev_virtio_ld
IOCONF=	VIRTIO_LD.ioconf

SRCS=	ld_at_virtio.c

SRCS+=	ld_virtio.c

# XXX: ld.c does not really belong here, but placing it in libdisk
# requires revamping config to be more modular in how CFDRIVER() is
# used.  moving ld up in the component tree later to libdisk won't
# create problems, so just put it here for now.  the excessive
# dependencies of virtio_ld should prevent anyone from want to
# provide ld via this component ...
SRCS+=	ld.c

CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern -I${RUMPTOP}/librump/rumpvfs
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
