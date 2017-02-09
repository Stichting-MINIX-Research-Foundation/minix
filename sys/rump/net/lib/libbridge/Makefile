#	$NetBSD: Makefile,v 1.4 2015/08/24 23:04:42 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../net

LIB=	rumpnet_bridge
IOCONF=	BRIDGE.ioconf
SRCS=	if_bridge.c bridgestp.c

SRCS+=	bridge_component.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
