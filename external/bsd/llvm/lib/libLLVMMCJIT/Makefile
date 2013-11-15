#	$NetBSD: Makefile,v 1.7 2013/01/23 15:02:59 joerg Exp $

LIB=	LLVMMCJIT

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/ExecutionEngine/MCJIT

SRCS+=	MCJIT.cpp \
	SectionMemoryManager.cpp

.include <bsd.lib.mk>
