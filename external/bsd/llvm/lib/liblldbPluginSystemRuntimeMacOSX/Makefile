#	$NetBSD: Makefile,v 1.1 2013/11/28 14:20:33 joerg Exp $

LIB=	lldbPluginSystemRuntimeMacOSX

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/SystemRuntime/MacOSX

SRCS+=	SystemRuntimeMacOSX.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
