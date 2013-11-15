#	$NetBSD: Makefile,v 1.2 2011/08/02 17:12:39 joerg Exp $

LIB=	LLVMMCParser

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/MC/MCParser

SRCS+=	AsmLexer.cpp \
	AsmParser.cpp \
	COFFAsmParser.cpp \
	DarwinAsmParser.cpp \
	ELFAsmParser.cpp \
	MCAsmLexer.cpp \
	MCAsmParser.cpp \
	MCAsmParserExtension.cpp \
	MCTargetAsmParser.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
