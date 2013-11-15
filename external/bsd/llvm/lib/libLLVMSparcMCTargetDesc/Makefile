#	$NetBSD: Makefile,v 1.1 2011/07/17 20:07:58 joerg Exp $

LIB=	LLVMSparcMCTargetDesc

.include <bsd.init.mk>

SPARC_OBJDIR!=	cd ${.CURDIR}/../libLLVMSparcCodeGen && ${PRINTOBJDIR}
CPPFLAGS+=	-I${SPARC_OBJDIR} -I${LLVM_SRCDIR}/lib/Target/Sparc

.PATH: ${LLVM_SRCDIR}/lib/Target/Sparc/MCTargetDesc

SRCS+=	SparcMCAsmInfo.cpp \
	SparcMCTargetDesc.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
