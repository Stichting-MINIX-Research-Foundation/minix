#	$NetBSD: Makefile,v 1.3 2013/05/28 17:37:34 joerg Exp $

LIB=	LLVMExecutionEngine

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/ExecutionEngine

SRCS+=	ExecutionEngine.cpp \
	ExecutionEngineBindings.cpp \
	RTDyldMemoryManager.cpp \
	TargetSelect.cpp

.include <bsd.lib.mk>
