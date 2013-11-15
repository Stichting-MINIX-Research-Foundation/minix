#	$NetBSD: Makefile,v 1.5 2013/10/14 01:37:39 joerg Exp $

LIB=	LLVMSparcCodeGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Target/Sparc

SRCS+=	DelaySlotFiller.cpp \
	SparcAsmPrinter.cpp \
	SparcCodeEmitter.cpp \
	SparcInstrInfo.cpp \
	SparcISelDAGToDAG.cpp \
	SparcISelLowering.cpp \
	SparcJITInfo.cpp \
	SparcFrameLowering.cpp \
	SparcMachineFunctionInfo.cpp \
	SparcRegisterInfo.cpp \
	SparcSubtarget.cpp \
	SparcTargetMachine.cpp \
	SparcSelectionDAGInfo.cpp


TABLEGEN_SRC=		Sparc.td
TABLEGEN_INCLUDES=	-I${LLVM_SRCDIR}/lib/Target/Sparc
TABLEGEN_OUTPUT= \
	SparcGenDAGISel.inc|-gen-dag-isel \
	SparcGenAsmWriter.inc|-gen-asm-writer \
	SparcGenCallingConv.inc|-gen-callingconv \
	SparcGenCodeEmitter.inc|-gen-emitter \
	SparcGenInstrInfo.inc|-gen-instr-info \
	SparcGenRegisterInfo.inc|-gen-register-info \
	SparcGenSubtargetInfo.inc|-gen-subtarget

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
