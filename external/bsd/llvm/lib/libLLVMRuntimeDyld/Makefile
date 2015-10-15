#	$NetBSD: Makefile,v 1.7 2015/03/18 17:43:20 joerg Exp $

LIB=	LLVMRuntimeDyld

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/ExecutionEngine/RuntimeDyld

SRCS+=	RTDyldMemoryManager.cpp \
	RuntimeDyld.cpp \
	RuntimeDyldChecker.cpp \
	RuntimeDyldELF.cpp \
	RuntimeDyldMachO.cpp

.include <bsd.lib.mk>
