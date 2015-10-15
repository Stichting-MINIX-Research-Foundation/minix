# $NetBSD: Makefile,v 1.9 2014/02/12 04:08:32 jmmv Exp $

USE_ATF_LIBTOOLS=	yes

.include <bsd.init.mk>

.PATH:		${SRCDIR}/tools

BINDIR=		/usr/bin
PROG_CXX=	atf-report
SRCS=		atf-report.cpp
MAN=		atf-report.1

CPPFLAGS+=	-I${SRCDIR}/tools

WARNS?=		2

CWARNFLAGS.clang+=	-Wno-error=unused-private-field

.include <bsd.prog.mk>
