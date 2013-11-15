#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:08 joerg Exp $

PROG_CXX=	ld.mcld
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/tools/llvm-mcld

SRCS=	llvm-mcld.cpp

MCLINKER_LIBS+= \
	ARMTargetInfo \
	ARMCodeGen \
	MipsTargetInfo \
	MipsCodeGen \
	X86TargetInfo \
	X86CodeGen \
	Target \
	CodeGen \
	Core \
	Object \
	Fragment \
	MC \
	ADT \
	Support \
	LD \

LLVM_LIBS+= \
	AsmParser \
	BitReader \
	AArch64CodeGen \
	AArch64TargetInfo \
	AArch64AsmParser \
	AArch64MCTargetDesc \
	AArch64AsmPrinter \
	AArch64Utils \
	ARMCodeGen \
	ARMTargetInfo \
	ARMAsmParser \
	ARMMCTargetDesc \
	ARMAsmPrinter \
	MipsCodeGen \
	MipsTargetInfo \
	MipsAsmParser \
	MipsDisassembler \
	MipsMCTargetDesc \
	MipsAsmPrinter \
	PowerPCCodeGen \
	PowerPCTargetInfo \
	PowerPCMCTargetDesc \
	PowerPCAsmPrinter \
	SparcCodeGen \
	SparcTargetInfo \
	SparcMCTargetDesc \
	X86CodeGen \
	X86TargetInfo \
	X86MCTargetDesc \
	X86AsmParser \
	X86AsmPrinter \
	X86Utils \
	SelectionDAG \
	CodeGen \
	Target \
	ScalarOpts \
	TransformsUtils \
	Analysis \
	AsmPrinter \
	MCParser \
	MC \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
