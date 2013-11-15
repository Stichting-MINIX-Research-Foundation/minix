#	$NetBSD: Makefile,v 1.1 2011/07/05 17:22:23 joerg Exp $

LIB=	LLVMARMTargetInfo

.include <bsd.init.mk>

ARM_OBJDIR!=	cd ${.CURDIR}/../libLLVMARMCodeGen && ${PRINTOBJDIR}
CPPFLAGS+=	-I${ARM_OBJDIR} -I${LLVM_SRCDIR}/lib/Target/ARM

.PATH: ${LLVM_SRCDIR}/lib/Target/ARM/TargetInfo

SRCS+=	ARMTargetInfo.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
