#	$NetBSD: Makefile,v 1.3 2013/02/04 15:00:35 joerg Exp $

LIB=	LLVMipa

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Analysis/IPA

SRCS+=	CallPrinter.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	FindUsedTypes.cpp \
	GlobalsModRef.cpp \
	IPA.cpp \
	InlineCost.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
