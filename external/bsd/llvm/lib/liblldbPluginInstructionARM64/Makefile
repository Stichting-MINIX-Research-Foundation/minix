#	$NetBSD: Makefile,v 1.1 2014/05/30 18:28:15 joerg Exp $

LIB=	lldbPluginInstructionARM64

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/Instruction/ARM64

SRCS+=	EmulateInstructionARM64.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
