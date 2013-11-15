#	$NetBSD: Makefile,v 1.4 2012/10/28 17:07:46 joerg Exp $

LIB=	LLVMTableGen

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/TableGen

SRCS+=	Error.cpp \
	Main.cpp \
	Record.cpp \
	StringMatcher.cpp \
	TableGenBackend.cpp \
	TGLexer.cpp \
	TGParser.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
