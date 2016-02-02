#	$NetBSD: Makefile,v 1.1 2014/05/30 18:28:16 joerg Exp $

LIB=	lldbPluginJITLoaderGDB

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/JITLoader/GDB

SRCS+=	JITLoaderGDB.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
