#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:11 joerg Exp $

LIB=	MCLinkerSupport

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Support

SRCS=	CommandLine.cpp \
	Directory.cpp \
	FileHandle.cpp \
	FileSystem.cpp \
	HandleToArea.cpp \
	LEB128.cpp \
	MemoryArea.cpp \
	MemoryAreaFactory.cpp \
	MemoryRegion.cpp \
	MsgHandling.cpp \
	Path.cpp \
	raw_mem_ostream.cpp \
	raw_ostream.cpp \
	RealPath.cpp \
	RegionFactory.cpp \
	Space.cpp \
	SystemUtils.cpp \
	TargetRegistry.cpp \
	ToolOutputFile.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
