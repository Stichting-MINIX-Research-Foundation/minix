#	$NetBSD: Makefile,v 1.4 2015/01/29 20:41:36 joerg Exp $

LIB=	LLVMipa

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Analysis/IPA

SRCS+=	CallPrinter.cpp \
	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	GlobalsModRef.cpp \
	IPA.cpp \
	InlineCost.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
