#	$NetBSD: Makefile,v 1.1 2014/05/30 18:28:16 joerg Exp $

LIB=	lldbPluginObjectFileJIT

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/ObjectFile/JIT

SRCS+=	ObjectFileJIT.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
