#	$NetBSD: Makefile,v 1.4 2013/11/28 14:20:00 joerg Exp $

LIB=	lldDriver

.include <bsd.init.mk>

.PATH: ${LLD_SRCDIR}/lib/Driver

SRCS+=	CoreDriver.cpp \
	DarwinLdDriver.cpp \
	Driver.cpp \
	GnuLdDriver.cpp \
	UniversalDriver.cpp \
	WinLinkDriver.cpp

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
