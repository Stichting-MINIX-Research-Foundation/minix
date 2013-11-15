#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:01 joerg Exp $

LIB=	lldbPluginObjectContainerBSDArchive

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/ObjectContainer/BSD-Archive

SRCS+=	ObjectContainerBSDArchive.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
