#	$NetBSD: Makefile,v 1.1 2011/02/06 01:13:56 joerg Exp $

LIB=	LLVMCppBackendInfo

CPPFLAGS+=	-I${LLVM_SRCDIR}/lib/Target/CppBackend

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/CppBackend/TargetInfo

SRCS+=	CppBackendTargetInfo.cpp

.include <bsd.lib.mk>
