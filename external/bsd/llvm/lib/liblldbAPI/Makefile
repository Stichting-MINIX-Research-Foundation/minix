#	$NetBSD: Makefile,v 1.4 2015/01/29 20:41:38 joerg Exp $

LIB=	lldbAPI

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/API

SRCS+=	SBAddress.cpp \
	SBBlock.cpp \
	SBBreakpoint.cpp \
	SBBreakpointLocation.cpp \
	SBBroadcaster.cpp \
	SBCommandInterpreter.cpp \
	SBCommandReturnObject.cpp \
	SBCommunication.cpp \
	SBCompileUnit.cpp \
	SBData.cpp \
	SBDebugger.cpp \
	SBDeclaration.cpp \
	SBError.cpp \
	SBEvent.cpp \
	SBExecutionContext.cpp \
	SBExpressionOptions.cpp \
	SBFileSpec.cpp \
	SBFileSpecList.cpp \
	SBFrame.cpp \
	SBFunction.cpp \
	SBHostOS.cpp \
	SBInstruction.cpp \
	SBInstructionList.cpp \
	SBLineEntry.cpp \
	SBListener.cpp \
	SBModule.cpp \
	SBModuleSpec.cpp \
	SBProcess.cpp \
	SBSection.cpp \
	SBSourceManager.cpp \
	SBStream.cpp \
	SBStringList.cpp \
	SBSymbol.cpp \
	SBSymbolContext.cpp \
	SBSymbolContextList.cpp \
	SBTarget.cpp \
	SBThread.cpp \
	SBThreadPlan.cpp \
	SBType.cpp \
	SBTypeCategory.cpp \
	SBTypeEnumMember.cpp \
	SBTypeFilter.cpp \
	SBTypeFormat.cpp \
	SBTypeNameSpecifier.cpp \
	SBTypeSummary.cpp \
	SBTypeSynthetic.cpp \
	SBUnixSignals.cpp \
	SBValue.cpp \
	SBValueList.cpp \
	SBWatchpoint.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
