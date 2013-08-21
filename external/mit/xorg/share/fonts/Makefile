#	$NetBSD: Makefile,v 1.5 2009/06/18 06:36:59 mrg Exp $

.include <bsd.own.mk>

.if ${MKX11FONTS} != "no"

SUBDIR=	encodings .WAIT \
	font-alias font-util \
	100dpi 75dpi Type1 TTF misc

.include "${NETBSDSRCDIR}/external/mit/xorg/tools/fc-cache/Makefile.fc-cache"

.endif

.include <bsd.subdir.mk>
