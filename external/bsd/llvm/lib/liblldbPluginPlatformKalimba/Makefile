#	$NetBSD: Makefile,v 1.1 2014/08/10 17:35:08 joerg Exp $

LIB=	lldbPluginPlatformKalimba

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Platform/Kalimba

SRCS+=	PlatformKalimba.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
