#	$NetBSD: Makefile,v 1.2 2014/05/30 18:28:14 joerg Exp $

LIB=	LLVMAArch64Disassembler

.include <bsd.init.mk>

AARCH64_OBJDIR!=	cd ${.CURDIR}/../libLLVMAArch64CodeGen && ${PRINTOBJDIR}
CPPFLAGS+=	-I${AARCH64_OBJDIR} -I${LLVM_SRCDIR}/lib/Target/AArch64

.PATH: ${LLVM_SRCDIR}/lib/Target/AArch64/Disassembler

SRCS+=	AArch64Disassembler.cpp \
	AArch64ExternalSymbolizer.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
