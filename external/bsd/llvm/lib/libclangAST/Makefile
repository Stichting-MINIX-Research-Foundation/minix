#	$NetBSD: Makefile,v 1.12 2013/10/14 01:37:39 joerg Exp $

LIB=	clangAST

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/AST

SRCS+=	APValue.cpp \
	ASTConsumer.cpp \
	ASTContext.cpp \
	ASTDiagnostic.cpp \
	ASTDumper.cpp \
	ASTImporter.cpp \
	ASTTypeTraits.cpp \
	AttrImpl.cpp \
	CXXInheritance.cpp \
	Comment.cpp \
	CommentBriefParser.cpp \
	CommentCommandTraits.cpp \
	CommentLexer.cpp \
	CommentParser.cpp \
	CommentSema.cpp \
	Decl.cpp \
	DeclarationName.cpp \
	DeclBase.cpp \
	DeclCXX.cpp \
	DeclFriend.cpp \
	DeclGroup.cpp \
	DeclObjC.cpp \
	DeclOpenMP.cpp \
	DeclPrinter.cpp \
	DeclTemplate.cpp \
	Expr.cpp \
	ExprClassification.cpp \
	ExprConstant.cpp \
	ExprCXX.cpp \
	ExternalASTSource.cpp \
	InheritViz.cpp \
	ItaniumCXXABI.cpp \
	ItaniumMangle.cpp \
	Mangle.cpp \
	MangleNumberingContext.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftMangle.cpp \
	NSAPI.cpp \
	NestedNameSpecifier.cpp \
	ParentMap.cpp \
	RawCommentList.cpp \
	RecordLayout.cpp \
	RecordLayoutBuilder.cpp \
	SelectorLocationsKind.cpp \
	Stmt.cpp \
	StmtIterator.cpp \
	StmtPrinter.cpp \
	StmtProfile.cpp \
	StmtViz.cpp \
	TemplateBase.cpp \
	TemplateName.cpp \
	Type.cpp \
	TypeLoc.cpp \
	TypePrinter.cpp \
	VTTBuilder.cpp \
	VTableBuilder.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
