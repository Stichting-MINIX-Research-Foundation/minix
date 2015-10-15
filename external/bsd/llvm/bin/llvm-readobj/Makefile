#	$NetBSD: Makefile,v 1.8 2014/08/10 17:34:20 joerg Exp $

PROG_CXX=	llvm-readobj
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-readobj

SRCS=	llvm-readobj.cpp \
	ARMAttributeParser.cpp \
	ARMWinEHPrinter.cpp \
	COFFDumper.cpp \
	ELFDumper.cpp \
	Error.cpp \
	MachODumper.cpp \
	ObjDumper.cpp \
	StreamWriter.cpp \
	Win64EHDumper.cpp

LLVM_LIBS+= \
	AArch64TargetInfo \
	ARMTargetInfo \
	MipsTargetInfo \
	PowerPCTargetInfo \
	SparcTargetInfo \
	X86TargetInfo \
	Object \
	MC \
	MCParser \
	BitReader \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
