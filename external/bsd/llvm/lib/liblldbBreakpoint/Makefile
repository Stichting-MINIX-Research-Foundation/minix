#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbBreakpoint

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Breakpoint

SRCS+=	Breakpoint.cpp \
	BreakpointID.cpp \
	BreakpointIDList.cpp \
	BreakpointList.cpp \
	BreakpointLocation.cpp \
	BreakpointLocationCollection.cpp \
	BreakpointLocationList.cpp \
	BreakpointOptions.cpp \
	BreakpointResolver.cpp \
	BreakpointResolverAddress.cpp \
	BreakpointResolverFileLine.cpp \
	BreakpointResolverFileRegex.cpp \
	BreakpointResolverName.cpp \
	BreakpointSite.cpp \
	BreakpointSiteList.cpp \
	Stoppoint.cpp \
	StoppointCallbackContext.cpp \
	StoppointLocation.cpp \
	Watchpoint.cpp \
	WatchpointList.cpp \
	WatchpointOptions.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
