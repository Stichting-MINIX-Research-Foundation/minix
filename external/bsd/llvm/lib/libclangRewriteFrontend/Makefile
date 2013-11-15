#	$NetBSD: Makefile,v 1.1 2012/09/23 17:22:24 joerg Exp $

LIB=	clangRewriteFrontend

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Rewrite/Frontend

SRCS+=	FixItRewriter.cpp \
	FrontendActions.cpp \
	HTMLPrint.cpp \
	InclusionRewriter.cpp \
	RewriteMacros.cpp \
	RewriteModernObjC.cpp \
	RewriteObjC.cpp \
	RewriteTest.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
