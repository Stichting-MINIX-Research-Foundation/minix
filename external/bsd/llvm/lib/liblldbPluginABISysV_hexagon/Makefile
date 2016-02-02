#	$NetBSD: Makefile,v 1.1 2014/08/10 17:35:07 joerg Exp $

LIB=	lldbPluginABISysV_hexagon

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/ABI/SysV-hexagon

SRCS+=	ABISysV_hexagon.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
