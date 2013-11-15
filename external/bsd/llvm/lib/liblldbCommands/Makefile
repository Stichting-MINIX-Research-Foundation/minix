#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbCommands

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Commands

SRCS+=	CommandCompletions.cpp \
	CommandObjectApropos.cpp \
	CommandObjectArgs.cpp \
	CommandObjectBreakpoint.cpp \
	CommandObjectBreakpointCommand.cpp \
	CommandObjectCommands.cpp \
	CommandObjectDisassemble.cpp \
	CommandObjectExpression.cpp \
	CommandObjectFrame.cpp \
	CommandObjectHelp.cpp \
	CommandObjectLog.cpp \
	CommandObjectMemory.cpp \
	CommandObjectMultiword.cpp \
	CommandObjectPlatform.cpp \
	CommandObjectPlugin.cpp \
	CommandObjectProcess.cpp \
	CommandObjectQuit.cpp \
	CommandObjectRegister.cpp \
	CommandObjectSettings.cpp \
	CommandObjectSource.cpp \
	CommandObjectSyntax.cpp \
	CommandObjectTarget.cpp \
	CommandObjectThread.cpp \
	CommandObjectType.cpp \
	CommandObjectVersion.cpp \
	CommandObjectWatchpoint.cpp \
	CommandObjectWatchpointCommand.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
