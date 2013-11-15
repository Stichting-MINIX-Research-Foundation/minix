#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:01 joerg Exp $

LIB=	lldbPluginPlatformMacOSX

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Platform/MacOSX

SRCS+=	PlatformDarwin.cpp \
	PlatformDarwinKernel.cpp \
	PlatformiOSSimulator.cpp \
	PlatformMacOSX.cpp \
	PlatformRemoteiOS.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
