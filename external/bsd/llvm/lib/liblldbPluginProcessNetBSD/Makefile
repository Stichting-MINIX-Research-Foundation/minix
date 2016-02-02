#	$NetBSD: Makefile,v 1.2 2014/05/30 18:28:16 joerg Exp $

LIB=	lldbPluginProcessNetBSD

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Process/FreeBSD

CPPFLAGS.ProcessFreeBSD.cpp+=	-I${LLDB_SRCDIR}/source/Plugins/Process/Utility
CPPFLAGS.ProcessMonitor.cpp+=	-I${LLDB_SRCDIR}/source/Plugins/Process/Utility

SRCS+=	ProcessFreeBSD.cpp \
	ProcessMonitor.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
