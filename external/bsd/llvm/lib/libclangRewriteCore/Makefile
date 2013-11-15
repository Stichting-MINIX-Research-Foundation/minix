#	$NetBSD: Makefile,v 1.1 2012/09/23 17:22:24 joerg Exp $

LIB=	clangRewriteCore

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Rewrite/Core

SRCS+=	DeltaTree.cpp \
	HTMLRewrite.cpp \
	RewriteRope.cpp \
	Rewriter.cpp \
	TokenRewriter.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
