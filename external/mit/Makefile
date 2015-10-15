#	$NetBSD: Makefile,v 1.5 2015/09/03 22:24:02 youri Exp $

.include <bsd.own.mk>

SUBDIR+=	expat lua

.if ${MKX11} != "no"
SUBDIR+=	xorg
SUBDIR+=	.WAIT ctwm
.endif

.include <bsd.subdir.mk>
