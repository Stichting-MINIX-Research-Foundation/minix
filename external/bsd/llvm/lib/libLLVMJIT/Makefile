#	$NetBSD: Makefile,v 1.5 2013/05/28 17:37:34 joerg Exp $

LIB=	LLVMJIT

.include <bsd.init.mk>

CPPFLAGS+=	-DENABLE_X86_JIT

.PATH: ${LLVM_SRCDIR}/lib/ExecutionEngine/JIT

SRCS+=	JIT.cpp \
	JITEmitter.cpp \
	JITMemoryManager.cpp

.include <bsd.lib.mk>
