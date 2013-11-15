#	$NetBSD: Makefile,v 1.1 2013/10/20 14:53:23 joerg Exp $

LIB=	lldbPluginPlatformWindows

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Platform/Windows

SRCS+=	PlatformWindows.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
