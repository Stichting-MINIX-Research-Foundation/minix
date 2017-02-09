#	$NetBSD: Makefile,v 1.5 2015/08/20 11:51:12 christos Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_cgd
IOCONF=	CGD.ioconf
SRCS=   cgd.c cgd_crypto.c

SRCS+=	cgd_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
