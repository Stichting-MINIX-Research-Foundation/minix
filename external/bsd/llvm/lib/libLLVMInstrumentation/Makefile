#	$NetBSD: Makefile,v 1.13 2015/01/29 20:41:35 joerg Exp $

LIB=	LLVMInstrumentation

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Transforms/Instrumentation

SRCS+=	AddressSanitizer.cpp \
	BoundsChecking.cpp \
	DataFlowSanitizer.cpp \
	GCOVProfiling.cpp \
	InstrProfiling.cpp \
	Instrumentation.cpp \
	MemorySanitizer.cpp \
	SanitizerCoverage.cpp \
	ThreadSanitizer.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
