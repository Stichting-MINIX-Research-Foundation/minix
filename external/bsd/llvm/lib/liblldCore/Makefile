#	$NetBSD: Makefile,v 1.4 2014/08/10 17:35:07 joerg Exp $

LIB=	lldCore

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Core

SRCS+=	DefinedAtom.cpp \
	Error.cpp \
	File.cpp \
	InputGraph.cpp \
	LinkingContext.cpp \
	Resolver.cpp \
	SymbolTable.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
