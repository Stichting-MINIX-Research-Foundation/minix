# $NetBSD: Makefile,v 1.3 2010/07/13 21:12:38 jmmv Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-config
ATFFILE=	yes

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-config

CPPFLAGS+=	-DHAVE_CONFIG_H

TESTS_SH=	integration_test

.include <bsd.test.mk>
