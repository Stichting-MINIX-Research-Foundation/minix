#	$NetBSD: Makefile,v 1.6 2013/09/02 14:34:55 joerg Exp $

PROG_CXX=	llvm-objdump
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-objdump

SRCS=	llvm-objdump.cpp \
	COFFDump.cpp \
	ELFDump.cpp \
	MachODump.cpp

LLVM_LIBS+= \
	AArch64Disassembler \
	AArch64TargetInfo \
	AArch64MCTargetDesc \
	AArch64AsmParser \
	AArch64AsmPrinter \
	AArch64Utils \
	ARMDisassembler \
	ARMTargetInfo \
	ARMMCTargetDesc \
	ARMAsmParser \
	ARMAsmPrinter \
	MipsDisassembler \
	MipsTargetInfo \
	MipsMCTargetDesc \
	MipsAsmPrinter \
	MipsAsmParser \
	PowerPCAsmParser \
	PowerPCTargetInfo \
	PowerPCMCTargetDesc \
	PowerPCAsmPrinter \
	SparcTargetInfo \
	SparcMCTargetDesc \
	X86Disassembler \
	X86MCTargetDesc \
	X86TargetInfo \
	X86AsmParser \
	X86AsmPrinter \
	X86Utils \
	DebugInfo \
	MCParser \
	MC \
	Object \
	IR \
	Support

LDADD+=	-lz
DPADD+=	${LIBZ}

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
