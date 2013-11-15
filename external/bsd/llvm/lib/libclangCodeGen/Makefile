#	$NetBSD: Makefile,v 1.7 2013/11/11 23:51:36 joerg Exp $

LIB=	clangCodeGen

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/CodeGen

SRCS+=	BackendUtil.cpp \
	CGAtomic.cpp \
	CGBlocks.cpp \
	CGBuiltin.cpp \
	CGCUDANV.cpp \
	CGCUDARuntime.cpp \
	CGCall.cpp \
	CGClass.cpp \
	CGCleanup.cpp \
	CGCXX.cpp \
	CGCXXABI.cpp \
	CGDebugInfo.cpp \
	CGDecl.cpp \
	CGDeclCXX.cpp \
	CGException.cpp \
	CGExpr.cpp \
	CGExprAgg.cpp \
	CGExprComplex.cpp \
	CGExprConstant.cpp \
	CGExprCXX.cpp \
	CGExprScalar.cpp \
	CGObjC.cpp \
	CGObjCGNU.cpp \
	CGObjCMac.cpp \
	CGObjCRuntime.cpp \
	CGOpenCLRuntime.cpp \
	CGRecordLayoutBuilder.cpp \
	CGRTTI.cpp \
	CGStmt.cpp \
	CGVTables.cpp \
	CGVTT.cpp \
	CodeGenABITypes.cpp \
	CodeGenAction.cpp \
	CodeGenFunction.cpp \
	CodeGenModule.cpp \
	CodeGenTBAA.cpp \
	CodeGenTypes.cpp \
	ItaniumCXXABI.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftVBTables.cpp \
	ModuleBuilder.cpp \
	TargetInfo.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
