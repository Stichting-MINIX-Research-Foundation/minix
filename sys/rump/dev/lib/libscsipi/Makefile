#	$NetBSD: Makefile,v 1.3 2015/08/24 23:21:51 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../dev/scsipi

LIB=	rumpdev_scsipi
IOCONF=	SCSIPI.ioconf

SRCS+=	scsipiconf.c scsipi_base.c scsipi_ioctl.c scsi_base.c scsiconf.c
SRCS+=	atapi_base.c atapiconf.c
SRCS+=	cd.c sd.c

.if 1
SRCS+=		scsipi_verbose.c
.endif

SRCS+=	scsipi_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
