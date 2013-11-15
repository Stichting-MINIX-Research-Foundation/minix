#	$NetBSD: Makefile,v 1.2 2013/11/11 23:51:36 joerg Exp $

LIB=	lldbPluginProcessUtility

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Process/Utility

SRCS+=	DynamicRegisterInfo.cpp \
	HistoryThread.cpp \
	HistoryUnwind.cpp \
	InferiorCallPOSIX.cpp \
	RegisterContextDarwin_arm.cpp \
	RegisterContextDarwin_i386.cpp \
	RegisterContextDarwin_x86_64.cpp \
	RegisterContextDummy.cpp \
	RegisterContextHistory.cpp \
	RegisterContextLLDB.cpp \
	RegisterContextMach_arm.cpp \
	RegisterContextMach_i386.cpp \
	RegisterContextMach_x86_64.cpp \
	RegisterContextMacOSXFrameBackchain.cpp \
	RegisterContextMemory.cpp \
	RegisterContextThreadMemory.cpp \
	StopInfoMachException.cpp \
	ThreadMemory.cpp \
	UnwindLLDB.cpp \
	UnwindMacOSXFrameBackchain.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
