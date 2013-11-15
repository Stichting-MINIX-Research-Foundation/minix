#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:02 joerg Exp $

LIB=	lldbSymbol

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Symbol

SRCS+=	Block.cpp \
	ClangASTContext.cpp \
	ClangASTImporter.cpp \
	ClangASTType.cpp \
	ClangExternalASTSourceCallbacks.cpp \
	ClangExternalASTSourceCommon.cpp \
	ClangNamespaceDecl.cpp \
	CompileUnit.cpp \
	Declaration.cpp \
	DWARFCallFrameInfo.cpp \
	Function.cpp \
	FuncUnwinders.cpp \
	LineEntry.cpp \
	LineTable.cpp \
	ObjectFile.cpp \
	Symbol.cpp \
	SymbolContext.cpp \
	SymbolFile.cpp \
	SymbolVendor.cpp \
	Symtab.cpp \
	Type.cpp \
	TypeList.cpp \
	UnwindPlan.cpp \
	UnwindTable.cpp \
	Variable.cpp \
	VariableList.cpp \
	VerifyDecl.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
