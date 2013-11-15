#	$NetBSD: Makefile,v 1.1 2013/05/28 17:37:35 joerg Exp $

LIB=	clangASTMatchersDynamic

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/ASTMatchers/Dynamic

SRCS+=	Diagnostics.cpp \
	Parser.cpp \
	Registry.cpp \
	VariantValue.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
