#	$NetBSD: Makefile,v 1.1 2011/02/06 01:14:07 joerg Exp $

LIB=	clangFrontendTool

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/FrontendTool

SRCS+=	ExecuteCompilerInvocation.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
