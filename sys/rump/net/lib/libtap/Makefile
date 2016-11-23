#	$NetBSD: Makefile,v 1.3 2015/08/24 23:04:43 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../net

LIB=	rumpnet_tap
IOCONF=	TAP.ioconf
SRCS=	if_tap.c

SRCS+=	tap_component.c

CPPFLAGS+=	-I${.CURDIR}/../../../librump/rumpvfs

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
