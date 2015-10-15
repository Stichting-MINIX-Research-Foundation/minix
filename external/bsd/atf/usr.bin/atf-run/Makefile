# $NetBSD: Makefile,v 1.14 2014/02/12 04:08:32 jmmv Exp $

USE_ATF_LIBTOOLS=	yes

.include <bsd.init.mk>

.PATH:		${SRCDIR}/tools
.PATH:		${SRCDIR}/tools/share

BINDIR=		/usr/bin
PROG_CXX=	atf-run
SRCS=		atf-run.cpp
MAN=		atf-run.1

CPPFLAGS+=	-DGDB=\"/usr/bin/gdb\"
CPPFLAGS+=	-I${SRCDIR}/tools

CWARNFLAGS.clang+=	-Wno-error=unused-private-field

COPTS.atf-run.cpp+=	-Wno-stack-protector

WARNS?=		2

.if ${MKSHARE} != "no"
FILESDIR=	/usr/share/atf
FILESMODE=	444
FILES=		atf-run.hooks
.endif

.include <bsd.prog.mk>
