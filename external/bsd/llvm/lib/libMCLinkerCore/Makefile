#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:09 joerg Exp $

LIB=	MCLinkerCore

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Core

SRCS=	AttributeOption.cpp \
	BitcodeOption.cpp \
	Environment.cpp \
	GeneralOptions.cpp \
	InputTree.cpp \
	IRBuilder.cpp \
	LinkerConfig.cpp \
	Linker.cpp \
	Module.cpp \
	ScriptOptions.cpp \
	TargetOptions.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
