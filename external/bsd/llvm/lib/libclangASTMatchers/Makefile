#	$NetBSD: Makefile,v 1.1 2012/08/09 14:39:01 joerg Exp $

LIB=	clangASTMatchers

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/ASTMatchers

SRCS+=	ASTMatchFinder.cpp \
	ASTMatchersInternal.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
