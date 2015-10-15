#	$NetBSD: Makefile,v 1.20 2015/01/29 20:41:35 joerg Exp $

LIB=	LLVMAnalysis

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Analysis

SRCS+=	AliasAnalysis.cpp \
	AliasAnalysisCounter.cpp \
	AliasAnalysisEvaluator.cpp \
	AliasDebugger.cpp \
	AliasSetTracker.cpp \
	Analysis.cpp \
	AssumptionCache.cpp \
	BasicAliasAnalysis.cpp \
	BlockFrequencyInfo.cpp \
	BlockFrequencyInfoImpl.cpp \
	BranchProbabilityInfo.cpp \
	CFG.cpp \
	CFGPrinter.cpp \
	CFLAliasAnalysis.cpp \
	CGSCCPassManager.cpp \
	CaptureTracking.cpp \
	CodeMetrics.cpp \
	ConstantFolding.cpp \
	CostModel.cpp \
	Delinearization.cpp \
	DependenceAnalysis.cpp \
	DomPrinter.cpp \
	DominanceFrontier.cpp \
	FunctionTargetTransformInfo.cpp \
	IVUsers.cpp \
	InstCount.cpp \
	InstructionSimplify.cpp \
	Interval.cpp \
	IntervalPartition.cpp \
	JumpInstrTableInfo.cpp \
	LazyCallGraph.cpp \
	LazyValueInfo.cpp \
	LibCallAliasAnalysis.cpp \
	LibCallSemantics.cpp \
	Lint.cpp \
	Loads.cpp \
	LoopInfo.cpp \
	LoopPass.cpp \
	MemDepPrinter.cpp \
	MemoryBuiltins.cpp \
	MemoryDependenceAnalysis.cpp \
	ModuleDebugInfoPrinter.cpp \
	NoAliasAnalysis.cpp \
	PHITransAddr.cpp \
	PostDominators.cpp \
	PtrUseVisitor.cpp \
	RegionInfo.cpp \
	RegionPass.cpp \
	RegionPrinter.cpp \
	ScalarEvolution.cpp \
	ScalarEvolutionAliasAnalysis.cpp \
	ScalarEvolutionExpander.cpp \
	ScalarEvolutionNormalization.cpp \
	ScopedNoAliasAA.cpp \
	SparsePropagation.cpp \
	TargetTransformInfo.cpp \
	Trace.cpp \
	TypeBasedAliasAnalysis.cpp \
	ValueTracking.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
