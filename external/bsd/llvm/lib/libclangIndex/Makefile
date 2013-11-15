#	$NetBSD: Makefile,v 1.5 2013/11/28 14:19:59 joerg Exp $

LIB=	clangIndex

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/Index

SRCS+=	CommentToXML.cpp \
	USRGeneration.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
