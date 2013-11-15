#	$NetBSD: Makefile,v 1.2 2013/10/14 01:37:40 joerg Exp $

LIB=	lldbPluginProcessMachCore

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Process/mach-core

CPPFLAGS.ProcessMachCore.cpp+=	-I${LLDB_SRCDIR}/source/Plugins/Process/Utility

SRCS+=	ProcessMachCore.cpp \
	ThreadMachCore.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
