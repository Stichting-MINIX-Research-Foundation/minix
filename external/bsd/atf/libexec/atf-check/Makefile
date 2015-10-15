# $NetBSD: Makefile,v 1.3 2014/02/12 04:08:31 jmmv Exp $

.include <bsd.init.mk>

.PATH:		${SRCDIR}/atf-sh

BINDIR=		/usr/libexec
PROG_CXX=	atf-check
SRCS=		atf-check.cpp
MAN=		atf-check.1

LDADD+=		-latf-c++ -latf-c
DPADD+=		${LIBATF_CXX} ${LIBATF_C}

WARNS?=		2

.include <bsd.prog.mk>
