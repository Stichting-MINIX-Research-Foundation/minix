#	$NetBSD: Makefile,v 1.5 2014/08/10 17:35:06 joerg Exp $

LIB=	clangRewrite

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Rewrite

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
