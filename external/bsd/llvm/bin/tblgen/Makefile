#	$NetBSD: Makefile,v 1.19 2014/08/10 17:34:20 joerg Exp $

PROG_CXX=	llvm-tblgen
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/utils/TableGen

SRCS=	AsmMatcherEmitter.cpp \
	AsmWriterEmitter.cpp \
	AsmWriterInst.cpp \
	CTagsEmitter.cpp \
	CallingConvEmitter.cpp \
	CodeEmitterGen.cpp \
	CodeGenDAGPatterns.cpp \
	CodeGenInstruction.cpp \
	CodeGenMapTable.cpp \
	CodeGenRegisters.cpp \
	CodeGenSchedule.cpp \
	CodeGenTarget.cpp \
	DAGISelEmitter.cpp \
	DAGISelMatcher.cpp \
	DAGISelMatcherEmitter.cpp \
	DAGISelMatcherGen.cpp \
	DAGISelMatcherOpt.cpp \
	DFAPacketizerEmitter.cpp \
	DisassemblerEmitter.cpp \
	FastISelEmitter.cpp \
	FixedLenDecoderEmitter.cpp \
	InstrInfoEmitter.cpp \
	IntrinsicEmitter.cpp \
	OptParserEmitter.cpp \
	PseudoLoweringEmitter.cpp \
	RegisterInfoEmitter.cpp \
	SubtargetEmitter.cpp \
	TableGen.cpp \
	X86DisassemblerTables.cpp \
	X86ModRMFilters.cpp \
	X86RecognizableInstr.cpp

LLVM_LIBS+= \
	TableGen \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
