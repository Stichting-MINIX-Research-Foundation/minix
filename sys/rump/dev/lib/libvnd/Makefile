#	$NetBSD: Makefile,v 1.2 2015/08/20 11:51:12 christos Exp $
#

.PATH:	${.CURDIR}/../../../../dev

LIB=	rumpdev_vnd
IOCONF=	VND.ioconf
SRCS=   vnd.c

SRCS+=	vnd_component.c

CPPFLAGS+=	-I${RUMPTOP}/librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
