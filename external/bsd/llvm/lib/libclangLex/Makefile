#	$NetBSD: Makefile,v 1.4 2013/01/23 15:03:01 joerg Exp $

LIB=	clangLex

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Lex

SRCS+=	HeaderMap.cpp \
	HeaderSearch.cpp \
	Lexer.cpp \
	LiteralSupport.cpp \
	MacroArgs.cpp \
	MacroInfo.cpp \
	ModuleMap.cpp \
	PPCaching.cpp \
	PPCallbacks.cpp \
	PPConditionalDirectiveRecord.cpp \
	PPDirectives.cpp \
	PPExpressions.cpp \
	PPLexerChange.cpp \
	PPMacroExpansion.cpp \
	PTHLexer.cpp \
	Pragma.cpp \
	PreprocessingRecord.cpp \
	Preprocessor.cpp \
	PreprocessorLexer.cpp \
	ScratchBuffer.cpp \
	TokenConcatenation.cpp \
	TokenLexer.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
