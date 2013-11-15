#	$NetBSD: Makefile,v 1.3 2013/11/11 23:51:36 joerg Exp $

LIB=	lldbTarget

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Target

CPPFLAGS.Thread.cpp+=	-I${LLDB_SRCDIR}/source/Plugins/Process/Utility

SRCS+=	ABI.cpp \
	CPPLanguageRuntime.cpp \
	ExecutionContext.cpp \
	LanguageRuntime.cpp \
	Memory.cpp \
	ObjCLanguageRuntime.cpp \
	OperatingSystem.cpp \
	PathMappingList.cpp \
	Platform.cpp \
	Process.cpp \
	RegisterContext.cpp \
	SectionLoadList.cpp \
	StackFrame.cpp \
	StackFrameList.cpp \
	StackID.cpp \
	StopInfo.cpp \
	SystemRuntime.cpp \
	Target.cpp \
	TargetList.cpp \
	Thread.cpp \
	ThreadList.cpp \
	ThreadPlan.cpp \
	ThreadPlanBase.cpp \
	ThreadPlanCallFunction.cpp \
	ThreadPlanCallUserExpression.cpp \
	ThreadPlanRunToAddress.cpp \
	ThreadPlanShouldStopHere.cpp \
	ThreadPlanStepInRange.cpp \
	ThreadPlanStepInstruction.cpp \
	ThreadPlanStepOut.cpp \
	ThreadPlanStepOverBreakpoint.cpp \
	ThreadPlanStepOverRange.cpp \
	ThreadPlanStepRange.cpp \
	ThreadPlanStepThrough.cpp \
	ThreadPlanStepUntil.cpp \
	ThreadPlanTracer.cpp \
	ThreadSpec.cpp \
	UnixSignals.cpp \
	UnwindAssembly.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
