# $NetBSD: Makefile,v 1.42 2015/01/29 20:41:34 joerg Exp $

.include <bsd.own.mk>

SUBDIR=	\
	libLLVMAnalysis \
	libLLVMAsmParser \
	libLLVMAsmPrinter \
	libLLVMBitReader \
	libLLVMBitWriter \
	libLLVMCodeGen \
	libLLVMIR \
	libLLVMIRReader \
	libLLVMInstCombine \
	libLLVMInstrumentation \
	libLLVMipa \
	libLLVMipo \
	libLLVMLinker \
	libLLVMMC \
	libLLVMMCDisassembler \
	libLLVMMCParser \
	libLLVMObjCARC \
	libLLVMObject \
	libLLVMOption \
	libLLVMProfileData \
	libLLVMScalarOpts \
	libLLVMSelectionDAG \
	libLLVMSupport \
	libLLVMTableGen \
	libLLVMTarget \
	libLLVMTransformsUtils \
	libLLVMVectorize \
	libLLVMAArch64CodeGen \
	libLLVMARMCodeGen \
	libLLVMMipsCodeGen \
	libLLVMPowerPCCodeGen \
	libLLVMSparcCodeGen \
	libLLVMX86CodeGen

SUBDIR+= .WAIT

SUBDIR+= \
	libLLVMAArch64AsmParser \
	libLLVMAArch64AsmPrinter \
	libLLVMAArch64Disassembler \
	libLLVMAArch64MCTargetDesc \
	libLLVMAArch64TargetInfo \
	libLLVMAArch64Utils

SUBDIR+= \
	libLLVMARMAsmParser \
	libLLVMARMAsmPrinter \
	libLLVMARMDisassembler \
	libLLVMARMMCTargetDesc \
	libLLVMARMTargetInfo

SUBDIR+= \
	libLLVMMipsAsmParser \
	libLLVMMipsAsmPrinter \
	libLLVMMipsDisassembler \
	libLLVMMipsMCTargetDesc \
	libLLVMMipsTargetInfo

SUBDIR+= \
	libLLVMPowerPCAsmParser \
	libLLVMPowerPCAsmPrinter \
	libLLVMPowerPCDisassembler \
	libLLVMPowerPCMCTargetDesc \
	libLLVMPowerPCTargetInfo

SUBDIR+= \
	libLLVMSparcAsmParser \
	libLLVMSparcAsmPrinter \
	libLLVMSparcDisassembler \
	libLLVMSparcMCTargetDesc \
	libLLVMSparcTargetInfo

SUBDIR+= \
	libLLVMX86AsmParser \
	libLLVMX86AsmPrinter \
	libLLVMX86Disassembler \
	libLLVMX86MCTargetDesc \
	libLLVMX86TargetInfo \
	libLLVMX86Utils

.if ${NO_LLVM_DEVELOPER:Uno} == "no" && (${LLVM_DEVELOPER:U} == "yes" || ${MKLLDB} == "yes")
SUBDIR+= \
	libLLVMExecutionEngine \
	libLLVMMCJIT \
	libLLVMRuntimeDyld
.endif

.if (${NO_LLVM_DEVELOPER:Uno} == "no" && ${LLVM_DEVELOPER:U} == "yes") || ${MKLLD} == "yes"
SUBDIR+= \
	libLLVMDebugInfo
.endif

.if ${NO_LLVM_DEVELOPER:Uno} == "no" && ${LLVM_DEVELOPER:U} == "yes"
SUBDIR+= \
	libLLVMCppBackend \
	libLLVMCppBackendInfo \
	libLLVMHello \
	libLLVMInterpreter \
	libLLVMLTO \
	libLLVMLineEditor
.endif

.if defined(__MINIX)
SUBDIR+= \
	libLLVMLTO \
	#libLLVMgold
.endif # defined(__MINIX)

SUBDIR+= \
	libclangAnalysis \
	libclangARCMigrate \
	libclangAST \
	libclangBasic \
	libclangCodeGen \
	libclangDriver \
	libclangEdit \
	libclangFrontend \
	libclangFrontendRewrite \
	libclangFrontendTool \
	libclangLex \
	libclangParse \
	libclangRewrite \
	libclangSema \
	libclangSerialization \
	libclangStaticAnalyzerCheckers \
	libclangStaticAnalyzerCore \
	.WAIT \
	libclangStaticAnalyzerFrontend

.if ${NO_LLVM_DEVELOPER:Uno} == "no" && ${LLVM_DEVELOPER:U} == "yes"
SUBDIR+= \
	libclangASTMatchers \
	libclangASTMatchersDynamic \
	libclangFormat \
	libclangIndex \
	libclangTooling \
	libclangToolingCore
.endif

.if ${MKLLD} != "no"
SUBDIR+= \
	liblldConfig \
	liblldCore \
	liblldDriver \
	liblldPasses \
	liblldReaderWriter \
	liblldReaderWriterELF \
	liblldReaderWriterELFAArch64 \
	liblldReaderWriterELFHexagon \
	liblldReaderWriterELFMips \
	liblldReaderWriterELFPPC \
	liblldReaderWriterELFX86 \
	liblldReaderWriterELFX86_64 \
	liblldReaderWriterMachO \
	liblldReaderWriterNative \
	liblldReaderWriterPECOFF \
	liblldReaderWriterYAML
.endif

.if ${MKMCLINKER} != "no"
SUBDIR+= \
	libMCLinkerADT \
	libMCLinkerCodeGen \
	libMCLinkerCore \
	libMCLinkerFragment \
	libMCLinkerLD \
	libMCLinkerMC \
	libMCLinkerObject \
	libMCLinkerSupport \
	libMCLinkerTarget

SUBDIR+= \
	libMCLinkerARMCodeGen libMCLinkerARMTargetInfo
SUBDIR+= \
	libMCLinkerMipsCodeGen libMCLinkerMipsTargetInfo
SUBDIR+= \
	libMCLinkerX86CodeGen libMCLinkerX86TargetInfo

.endif

.if ${NO_LLVM_DEVELOPER:Uno} == "no" && ${MKLLDB} == "yes"
SUBDIR+= \
	liblldbAPI \
	liblldbBreakpoint \
	liblldbCommands \
	liblldbCore \
	liblldbDataFormatters \
	liblldbExpression \
	liblldbHostCommon \
	liblldbHostNetBSD \
	liblldbInterpreter \
	liblldbLog \
	liblldbPluginABIMacOSX_arm \
	liblldbPluginABIMacOSX_arm64 \
	liblldbPluginABIMacOSX_i386 \
	liblldbPluginABISysV_hexagon \
	liblldbPluginABISysV_ppc \
	liblldbPluginABISysV_ppc64 \
	liblldbPluginABISysV_x86_64 \
	liblldbPluginAppleObjCRuntime \
	liblldbPluginCXXItaniumABI \
	liblldbPluginDisassemblerLLVM \
	liblldbPluginDynamicLoaderHexagonDYLD \
	liblldbPluginDynamicLoaderMacOSXDYLD \
	liblldbPluginDynamicLoaderPosixDYLD \
	liblldbPluginDynamicLoaderStatic \
	liblldbPluginInstructionARM \
	liblldbPluginInstructionARM64 \
	liblldbPluginJITLoaderGDB \
	liblldbPluginObjectContainerBSDArchive \
	liblldbPluginObjectContainerMachOArchive \
	liblldbPluginObjectFileELF \
	liblldbPluginObjectFileJIT \
	liblldbPluginObjectFileMachO \
	liblldbPluginObjectFilePECOFF \
	liblldbPluginPlatformNetBSD \
	liblldbPluginPlatformGDB \
	liblldbPluginPlatformKalimba \
	liblldbPluginPlatformLinux \
	liblldbPluginPlatformMacOSX \
	liblldbPluginPlatformPOSIX \
	liblldbPluginPlatformWindows \
	liblldbPluginProcessElfCore \
	liblldbPluginProcessNetBSD \
	liblldbPluginProcessGDBRemote \
	liblldbPluginProcessMachCore \
	liblldbPluginProcessPOSIX \
	liblldbPluginProcessUtility \
	liblldbPluginSymbolFileDWARF \
	liblldbPluginSymbolFileSymtab \
	liblldbPluginSymbolVendorELF \
	liblldbPluginSystemRuntimeMacOSX \
	liblldbPluginUnwindAssemblyInstEmulation \
	liblldbPluginUnwindAssemblyX86 \
	liblldbSymbol \
	liblldbTarget \
	liblldbUtility
.endif

.include <bsd.subdir.mk>
