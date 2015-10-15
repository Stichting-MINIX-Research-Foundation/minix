#	$NetBSD: Makefile,v 1.4 2014/08/10 17:34:20 joerg Exp $

PROG_CXX=	lldb
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/tools/driver

SRCS=	Driver.cpp \
	Platform.cpp

CPPFLAGS.Driver.cpp+=	-Dgetopt_long_only=getopt_long

LLDB_LIBS+= \
	API \
	Log \
	PluginInstructionARM \
	PluginInstructionARM64 \
	PluginProcessGDBRemote \
	PluginProcessMachCore \
	PluginProcessPOSIX \
	PluginProcessUtility \
	PluginSymbolFileDWARF \
	PluginSymbolFileSymtab \
	PluginSymbolVendorELF \
	PluginUnwindAssemblyInstEmulation \
	PluginUnwindAssemblyX86 \
	Breakpoint \
	Expression \
	Symbol \
	Target \
	Utility \
	HostCommon \
	Interpreter \
	Commands \
	Breakpoint \
	Core \
	DataFormatters \
	PluginABIMacOSX_arm \
	PluginABIMacOSX_arm64 \
	PluginABIMacOSX_i386 \
	PluginABISysV_x86_64 \
	PluginAppleObjCRuntime \
	PluginCXXItaniumABI \
	PluginDisassemblerLLVM \
	PluginDynamicLoaderMacOSXDYLD \
	PluginDynamicLoaderPosixDYLD \
	PluginDynamicLoaderStatic \
	PluginJITLoaderGDB \
	PluginObjectContainerBSDArchive \
	PluginObjectContainerMachOArchive \
	PluginObjectFileELF \
	PluginObjectFileJIT \
	PluginObjectFileMachO \
	PluginObjectFilePECOFF \
	PluginPlatformNetBSD \
	PluginPlatformGDB \
	PluginPlatformKalimba \
	PluginPlatformLinux \
	PluginPlatformMacOSX \
	PluginPlatformPOSIX \
	PluginPlatformWindows \
	PluginProcessElfCore \
	PluginProcessNetBSD \
	PluginProcessPOSIX \
	PluginProcessUtility \
	HostCommon \
	HostNetBSD \
	Interpreter \

CLANG_LIBS+= \
	clangFrontend \
	clangDriver \
	clangSerialization \
	clangCodeGen \
	clangParse \
	clangSema \
	clangEdit \
	clangAnalysis \
	clangAST \
	clangLex \
	clangBasic

LLVM_LIBS+= \
	JIT \
	MCJIT \
	RuntimeDyld \
	ExecutionEngine \
	ObjCARC \
	ipo \
	IRReader \
	AsmParser \
	MCDisassembler \
	AArch64CodeGen \
	AArch64TargetInfo \
	AArch64AsmParser \
	AArch64MCTargetDesc \
	AArch64AsmPrinter \
	AArch64Disassembler \
	AArch64Utils \
	ARMCodeGen \
	ARMTargetInfo \
	ARMAsmParser \
	ARMMCTargetDesc \
	ARMAsmPrinter \
	ARMDisassembler \
	MipsCodeGen \
	MipsTargetInfo \
	MipsAsmParser \
	MipsDisassembler \
	MipsMCTargetDesc \
	MipsAsmPrinter \
	PowerPCCodeGen \
	PowerPCAsmParser \
	PowerPCTargetInfo \
	PowerPCDisassembler \
	PowerPCMCTargetDesc \
	PowerPCAsmPrinter \
	SparcCodeGen \
	SparcTargetInfo \
	SparcDisassembler \
	SparcMCTargetDesc \
	SparcAsmParser \
	SparcAsmPrinter \
	X86CodeGen \
	X86TargetInfo \
	X86MCTargetDesc \
	X86AsmParser \
	X86Disassembler \
	X86AsmPrinter \
	X86Utils \
	SelectionDAG \
	AsmPrinter \
	CodeGen \
	Vectorize \
	Target \
	InstCombine \
	Instrumentation \
	ScalarOpts \
	MCDisassembler \
	MCParser \
	MC \
	Object \
	BitReader \
	BitWriter \
	Linker \
	TransformsUtils \
	Analysis \
	ipa \
	IR \
	ProfileData \
	Option \
	Support

LDADD+=	-ledit -lterminfo -lexecinfo -lz
DPADD+=	${LIBEDIT} ${LIBTERMINFO} ${LIBEXECINFO} ${LIBZ}
DBG=-g

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
