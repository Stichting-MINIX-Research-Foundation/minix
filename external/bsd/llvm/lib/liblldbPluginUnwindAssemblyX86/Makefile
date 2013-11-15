#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:02 joerg Exp $

LIB=	lldbPluginUnwindAssemblyX86

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/UnwindAssembly/x86

SRCS+=	UnwindAssembly-x86.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
