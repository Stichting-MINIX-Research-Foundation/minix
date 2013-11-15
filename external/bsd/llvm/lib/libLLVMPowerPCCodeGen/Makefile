#	$NetBSD: Makefile,v 1.8 2013/09/02 14:34:56 joerg Exp $

LIB=	LLVMPowerPCCodeGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/PowerPC

SRCS+=	PPCAsmPrinter.cpp \
	PPCBranchSelector.cpp \
	PPCCTRLoops.cpp \
	PPCCodeEmitter.cpp \
	PPCHazardRecognizers.cpp \
	PPCInstrInfo.cpp \
	PPCISelDAGToDAG.cpp \
	PPCISelLowering.cpp \
	PPCFastISel.cpp \
	PPCFrameLowering.cpp \
	PPCJITInfo.cpp \
	PPCMachineFunctionInfo.cpp \
	PPCMCInstLower.cpp \
	PPCRegisterInfo.cpp \
	PPCSelectionDAGInfo.cpp \
	PPCSubtarget.cpp \
	PPCTargetMachine.cpp \
	PPCTargetObjectFile.cpp \
	PPCTargetTransformInfo.cpp

TABLEGEN_SRC=		PPC.td
TABLEGEN_INCLUDES=	-I${LLVM_SRCDIR}/lib/Target/PowerPC
TABLEGEN_OUTPUT= \
	PPCGenAsmWriter.inc|-gen-asm-writer \
	PPCGenAsmMatcher.inc|-gen-asm-matcher \
	PPCGenCodeEmitter.inc|-gen-emitter \
	PPCGenFastISel.inc|-gen-fast-isel \
	PPCGenMCCodeEmitter.inc|-gen-emitter^-mc-emitter \
	PPCGenRegisterInfo.inc|-gen-register-info \
	PPCGenInstrInfo.inc|-gen-instr-info \
	PPCGenDAGISel.inc|-gen-dag-isel \
	PPCGenCallingConv.inc|-gen-callingconv \
	PPCGenSubtargetInfo.inc|-gen-subtarget

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
