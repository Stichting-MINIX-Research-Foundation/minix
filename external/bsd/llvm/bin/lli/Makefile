#	$NetBSD: Makefile,v 1.11 2013/10/14 01:37:39 joerg Exp $

PROG_CXX=	lli
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/lli

SRCS=	lli.cpp \
	RemoteMemoryManager.cpp \
	RemoteTarget.cpp \
	RemoteTargetExternal.cpp

LLVM_LIBS+= \
	X86CodeGen \
	X86TargetInfo \
	X86MCTargetDesc \
	X86AsmPrinter \
	X86Utils \
	AsmPrinter \
	IRReader \
	AsmParser \
	MCParser \
	BitReader \
	JIT \
	MCJIT \
	RuntimeDyld \
	Instrumentation \
	Interpreter \
	ExecutionEngine \
	Object \
	SelectionDAG \
	CodeGen \
	ScalarOpts \
	TransformsUtils \
	Analysis \
	Target \
	MC \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
