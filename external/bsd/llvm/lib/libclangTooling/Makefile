#	$NetBSD: Makefile,v 1.8 2012/10/28 17:07:47 joerg Exp $

LIB=	clangTooling

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Tooling

SRCS+=	ArgumentsAdjusters.cpp \
	CommonOptionsParser.cpp \
	CompilationDatabase.cpp \
	FileMatchTrie.cpp \
	JSONCompilationDatabase.cpp \
	Refactoring.cpp \
	RefactoringCallbacks.cpp \
	Tooling.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
