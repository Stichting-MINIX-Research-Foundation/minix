#	$NetBSD: Makefile,v 1.1 2014/08/10 17:34:22 joerg Exp $

LIB=	clangFrontendRewrite

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Frontend/Rewrite

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
