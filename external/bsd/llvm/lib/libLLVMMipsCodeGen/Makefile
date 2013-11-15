#	$NetBSD: Makefile,v 1.20 2013/09/02 14:34:56 joerg Exp $

LIB=	LLVMMipsCodeGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/Mips

SRCS+=	Mips16ISelDAGToDAG.cpp \
	Mips16ISelLowering.cpp \
	Mips16InstrInfo.cpp \
	Mips16FrameLowering.cpp \
	Mips16HardFloat.cpp \
	Mips16RegisterInfo.cpp \
	MipsAnalyzeImmediate.cpp \
	MipsAsmPrinter.cpp \
	MipsCodeEmitter.cpp \
	MipsConstantIslandPass.cpp \
	MipsDelaySlotFiller.cpp \
	MipsFrameLowering.cpp \
	MipsInstrInfo.cpp \
	MipsISelDAGToDAG.cpp \
	MipsISelLowering.cpp \
	MipsJITInfo.cpp \
	MipsLongBranch.cpp \
	MipsMachineFunction.cpp \
	MipsModuleISelDAGToDAG.cpp \
	MipsMCInstLower.cpp \
	MipsOs16.cpp \
	MipsRegisterInfo.cpp \
	MipsSEISelLowering.cpp \
	MipsSEISelDAGToDAG.cpp \
	MipsSEInstrInfo.cpp \
	MipsSEFrameLowering.cpp \
	MipsSERegisterInfo.cpp \
	MipsSubtarget.cpp \
	MipsTargetMachine.cpp \
	MipsTargetObjectFile.cpp \
	MipsSelectionDAGInfo.cpp


TABLEGEN_SRC=		Mips.td
TABLEGEN_INCLUDES=	-I${LLVM_SRCDIR}/lib/Target/Mips
TABLEGEN_OUTPUT= \
	MipsGenRegisterInfo.inc|-gen-register-info \
	MipsGenInstrInfo.inc|-gen-instr-info \
	MipsGenAsmMatcher.inc|-gen-asm-matcher \
	MipsGenAsmWriter.inc|-gen-asm-writer \
	MipsGenCallingConv.inc|-gen-callingconv \
	MipsGenCodeEmitter.inc|-gen-emitter \
	MipsGenDAGISel.inc|-gen-dag-isel \
	MipsGenDisassemblerTables.inc|-gen-disassembler \
	MipsGenMCCodeEmitter.inc|-gen-emitter^-mc-emitter \
	MipsGenMCPseudoLowering.inc|-gen-pseudo-lowering \
	MipsGenSubtargetInfo.inc|-gen-subtarget

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
