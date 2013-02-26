# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:50 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

.PATH:		${SRCDIR}/store

LIB=		store

WARNS?=		3

SRCS=		backend.cpp \
		dbtypes.cpp \
		exceptions.cpp \
		metadata.cpp \
		transaction.cpp

.include <bsd.lib.mk>
