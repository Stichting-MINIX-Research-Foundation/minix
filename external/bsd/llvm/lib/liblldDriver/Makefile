#	$NetBSD: Makefile,v 1.7 2015/01/29 20:41:37 joerg Exp $

LIB=	lldDriver

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Driver

SRCS+=	CoreDriver.cpp \
	DarwinLdDriver.cpp \
	Driver.cpp \
	DarwinInputGraph.cpp \
	GnuLdDriver.cpp \
	GnuLdInputGraph.cpp \
	UniversalDriver.cpp \
	WinLinkDriver.cpp \
	WinLinkInputGraph.cpp \
	WinLinkModuleDef.cpp

TABLEGEN_SRC=	CoreOptions.td DarwinLdOptions.td GnuLdOptions.td \
	UniversalDriverOptions.td WinLinkOptions.td

TABLEGEN_OUTPUT.UniversalDriverOptions.td= \
	UniversalDriverOptions.inc|-gen-opt-parser-defs
TABLEGEN_OUTPUT.CoreOptions.td= \
	CoreOptions.inc|-gen-opt-parser-defs
TABLEGEN_OUTPUT.DarwinLdOptions.td= \
	DarwinLdOptions.inc|-gen-opt-parser-defs
TABLEGEN_OUTPUT.GnuLdOptions.td= \
	GnuLdOptions.inc|-gen-opt-parser-defs
TABLEGEN_OUTPUT.WinLinkOptions.td= \
	WinLinkOptions.inc|-gen-opt-parser-defs

.include "${.PARSEDIR}/../../tablegen.mk"

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
