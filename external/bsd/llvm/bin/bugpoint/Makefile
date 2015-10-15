#	$NetBSD: Makefile,v 1.8 2015/01/29 20:41:33 joerg Exp $

PROG_CXX=	bugpoint
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/bugpoint

SRCS=	BugDriver.cpp \
	CrashDebugger.cpp \
	ExecutionDriver.cpp \
	ExtractFunction.cpp \
	FindBugs.cpp \
	Miscompilation.cpp \
	OptimizerDriver.cpp \
	ToolRunner.cpp \
	bugpoint.cpp

LLVM_LIBS+= \
	CodeGen \
	ObjCARC \
	IRReader \
	AsmParser \
	BitReader \
	BitWriter \
	InstCombine \
	Instrumentation \
	MC \
	Linker \
	ScalarOpts \
	ProfileData \
	Target \
	ipo \
	Vectorize \
	TransformsUtils \
	Analysis \
	ipa \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
