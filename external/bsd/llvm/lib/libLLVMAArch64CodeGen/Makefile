#	$NetBSD: Makefile,v 1.6 2015/01/29 20:41:34 joerg Exp $

LIB=	LLVMAArch64CodeGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/AArch64

CPPFLAGS+=	-I${LLVM_SRCDIR}/lib/Target/AArch64

SRCS+=	AArch64A53Fix835769.cpp \
	AArch64A57FPLoadBalancing.cpp \
	AArch64AddressTypePromotion.cpp \
	AArch64AdvSIMDScalarPass.cpp \
	AArch64AsmPrinter.cpp \
	AArch64BranchRelaxation.cpp \
	AArch64CleanupLocalDynamicTLSPass.cpp \
	AArch64CollectLOH.cpp \
	AArch64ConditionOptimizer.cpp \
	AArch64ConditionalCompares.cpp \
	AArch64DeadRegisterDefinitionsPass.cpp \
	AArch64ExpandPseudoInsts.cpp \
	AArch64FastISel.cpp \
	AArch64FrameLowering.cpp \
	AArch64InstrInfo.cpp \
	AArch64ISelDAGToDAG.cpp \
	AArch64ISelLowering.cpp \
	AArch64LoadStoreOptimizer.cpp \
	AArch64MCInstLower.cpp \
	AArch64PBQPRegAlloc.cpp \
	AArch64PromoteConstant.cpp \
	AArch64RegisterInfo.cpp \
	AArch64SelectionDAGInfo.cpp \
	AArch64StorePairSuppress.cpp \
	AArch64Subtarget.cpp \
	AArch64TargetMachine.cpp \
	AArch64TargetObjectFile.cpp \
	AArch64TargetTransformInfo.cpp

TABLEGEN_SRC=		AArch64.td
TABLEGEN_INCLUDES=	-I${LLVM_SRCDIR}/lib/Target/AArch64
TABLEGEN_OUTPUT= \
	AArch64GenAsmMatcher.inc|-gen-asm-matcher \
	AArch64GenAsmWriter1.inc|-gen-asm-writer^-asmwriternum=1 \
	AArch64GenAsmWriter.inc|-gen-asm-writer \
	AArch64GenCallingConv.inc|-gen-callingconv \
	AArch64GenDAGISel.inc|-gen-dag-isel \
	AArch64GenDisassemblerTables.inc|-gen-disassembler \
	AArch64GenFastISel.inc|-gen-fast-isel \
	AArch64GenInstrInfo.inc|-gen-instr-info \
	AArch64GenMCCodeEmitter.inc|-gen-emitter \
	AArch64GenMCPseudoLowering.inc|-gen-pseudo-lowering \
	AArch64GenRegisterInfo.inc|-gen-register-info \
	AArch64GenSubtargetInfo.inc|-gen-subtarget

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
