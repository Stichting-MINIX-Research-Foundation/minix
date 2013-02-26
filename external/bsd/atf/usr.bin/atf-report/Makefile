# $NetBSD: Makefile,v 1.6 2012/06/16 17:02:33 joerg Exp $

.include <bsd.own.mk>

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-report

BINDIR=		/usr/bin
PROG_CXX=	atf-report
SRCS=		atf-report.cpp reader.cpp
MAN=		atf-report.1

LDADD+=		-latf-c++ -latf-c
DPADD+=		${LIBATF_CXX} ${LIBATF_C}

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${SRCDIR}

WARNS?=		2

CWARNFLAGS.clang+=	-Wno-error=unused-private-field

.include <bsd.prog.mk>
