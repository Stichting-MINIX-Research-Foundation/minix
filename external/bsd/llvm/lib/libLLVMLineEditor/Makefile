#	$NetBSD: Makefile,v 1.1 2014/02/14 20:32:58 joerg Exp $

LIB=	LLVMLineEditor

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/LineEditor

SRCS+=	LineEditor.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
