#	$NetBSD: Makefile,v 1.5 2013/01/23 15:03:00 joerg Exp $

LIB=	clangARCMigrate

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/ARCMigrate

SRCS+=	ARCMT.cpp \
	ARCMTActions.cpp \
	ObjCMT.cpp \
	FileRemapper.cpp \
	PlistReporter.cpp \
	TransAPIUses.cpp \
	TransARCAssign.cpp \
	TransAutoreleasePool.cpp \
	TransBlockObjCVariable.cpp \
	TransEmptyStatementsAndDealloc.cpp \
	TransGCAttrs.cpp \
	TransGCCalls.cpp \
	TransProperties.cpp \
	TransProtectedScope.cpp \
	TransRetainReleaseDealloc.cpp \
	TransUnbridgedCasts.cpp \
	TransUnusedInitDelegate.cpp \
	TransZeroOutPropsInDealloc.cpp \
	Transforms.cpp \
	TransformActions.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
