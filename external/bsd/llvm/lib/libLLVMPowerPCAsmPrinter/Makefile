#	$NetBSD: Makefile,v 1.1 2011/07/04 22:37:59 joerg Exp $

LIB=	LLVMPowerPCAsmPrinter
NOGCCERROR=	yes

.include <bsd.init.mk>

PPC_OBJDIR!=	cd ${.CURDIR}/../libLLVMPowerPCCodeGen && ${PRINTOBJDIR}
CPPFLAGS+=	-I${PPC_OBJDIR} -I${LLVM_SRCDIR}/lib/Target/PowerPC

.PATH: ${LLVM_SRCDIR}/lib/Target/PowerPC/InstPrinter

SRCS+=	PPCInstPrinter.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
