#	$NetBSD: Makefile,v 1.3 2013/11/28 14:19:59 joerg Exp $

LIB=	lldCore

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Core

SRCS+=	DefinedAtom.cpp \
	Error.cpp \
	File.cpp \
	InputGraph.cpp \
	LinkingContext.cpp \
	PassManager.cpp \
	Resolver.cpp \
	SymbolTable.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
