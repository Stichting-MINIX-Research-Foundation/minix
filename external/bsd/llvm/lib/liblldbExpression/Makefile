#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	lldbExpression

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Expression

SRCS+=	ASTDumper.cpp \
	ASTResultSynthesizer.cpp \
	ASTStructExtractor.cpp \
	ClangASTSource.cpp \
	ClangExpressionDeclMap.cpp \
	ClangExpressionParser.cpp \
	ClangExpressionVariable.cpp \
	ClangFunction.cpp \
	ClangPersistentVariables.cpp \
	ClangUserExpression.cpp \
	ClangUtilityFunction.cpp \
	DWARFExpression.cpp \
	ExpressionSourceCode.cpp \
	IRDynamicChecks.cpp \
	IRExecutionUnit.cpp \
	IRForTarget.cpp \
	IRInterpreter.cpp \
	IRMemoryMap.cpp \
	Materializer.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
