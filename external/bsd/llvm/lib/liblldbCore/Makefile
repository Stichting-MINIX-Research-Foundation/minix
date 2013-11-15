#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbCore

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Core

SRCS+=	Address.cpp \
	AddressRange.cpp \
	AddressResolver.cpp \
	AddressResolverFileLine.cpp \
	AddressResolverName.cpp \
	ArchSpec.cpp \
	Baton.cpp \
	Broadcaster.cpp \
	Communication.cpp \
	Connection.cpp \
	ConnectionFileDescriptor.cpp \
	ConnectionMachPort.cpp \
	ConnectionSharedMemory.cpp \
	ConstString.cpp \
	DataBufferHeap.cpp \
	DataBufferMemoryMap.cpp \
	DataEncoder.cpp \
	DataExtractor.cpp \
	Debugger.cpp \
	Disassembler.cpp \
	DynamicLoader.cpp \
	EmulateInstruction.cpp \
	Error.cpp \
	Event.cpp \
	FileLineResolver.cpp \
	FileSpecList.cpp \
	History.cpp \
	InputReader.cpp \
	InputReaderEZ.cpp \
	InputReaderStack.cpp \
	Language.cpp \
	Listener.cpp \
	Log.cpp \
	Mangled.cpp \
	Module.cpp \
	ModuleChild.cpp \
	ModuleList.cpp \
	Opcode.cpp \
	PluginManager.cpp \
	RegisterValue.cpp \
	RegularExpression.cpp \
	Scalar.cpp \
	SearchFilter.cpp \
	Section.cpp \
	SourceManager.cpp \
	State.cpp \
	Stream.cpp \
	StreamAsynchronousIO.cpp \
	StreamCallback.cpp \
	StreamFile.cpp \
	StreamGDBRemote.cpp \
	StreamString.cpp \
	StringList.cpp \
	Timer.cpp \
	UserID.cpp \
	UserSettingsController.cpp \
	UUID.cpp \
	Value.cpp \
	ValueObject.cpp \
	ValueObjectCast.cpp \
	ValueObjectChild.cpp \
	ValueObjectConstResult.cpp \
	ValueObjectConstResultChild.cpp \
	ValueObjectConstResultImpl.cpp \
	ValueObjectDynamicValue.cpp \
	ValueObjectList.cpp \
	ValueObjectMemory.cpp \
	ValueObjectRegister.cpp \
	ValueObjectSyntheticFilter.cpp \
	ValueObjectVariable.cpp \
	VMRange.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
