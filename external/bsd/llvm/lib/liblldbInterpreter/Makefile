#	$NetBSD: Makefile,v 1.3 2015/01/29 20:41:38 joerg Exp $

LIB=	lldbInterpreter

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Interpreter

SRCS+=	Args.cpp \
	CommandHistory.cpp \
	CommandInterpreter.cpp \
	CommandObject.cpp \
	CommandObjectRegexCommand.cpp \
	CommandObjectScript.cpp \
	CommandOptionValidators.cpp \
	CommandReturnObject.cpp \
	OptionGroupArchitecture.cpp \
	OptionGroupBoolean.cpp \
	OptionGroupFile.cpp \
	OptionGroupFormat.cpp \
	OptionGroupOutputFile.cpp \
	OptionGroupPlatform.cpp \
	OptionGroupString.cpp \
	OptionGroupUInt64.cpp \
	OptionGroupUUID.cpp \
	OptionGroupValueObjectDisplay.cpp \
	OptionValue.cpp \
	OptionValueArch.cpp \
	OptionValueArgs.cpp \
	OptionValueArray.cpp \
	OptionValueBoolean.cpp \
	OptionValueChar.cpp \
	OptionValueDictionary.cpp \
	OptionValueEnumeration.cpp \
	OptionValueFileSpec.cpp \
	OptionValueFileSpecLIst.cpp \
	OptionValueFormat.cpp \
	OptionValuePathMappings.cpp \
	OptionValueProperties.cpp \
	OptionValueRegex.cpp \
	OptionValueSInt64.cpp \
	OptionValueString.cpp \
	OptionValueUInt64.cpp \
	OptionValueUUID.cpp \
	OptionGroupVariable.cpp \
	OptionGroupWatchpoint.cpp \
	Options.cpp \
	Property.cpp \
	PythonDataObjects.cpp \
	ScriptInterpreter.cpp \
	ScriptInterpreterNone.cpp \
	ScriptInterpreterPython.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
