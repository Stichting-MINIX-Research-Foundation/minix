#	$NetBSD: Makefile,v 1.5 2015/03/18 17:43:20 joerg Exp $

LIB=	LLVMExecutionEngine

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/ExecutionEngine

SRCS+=	GDBRegistrationListener.cpp \
	ExecutionEngine.cpp \
	ExecutionEngineBindings.cpp \
	TargetSelect.cpp

.include <bsd.lib.mk>
