# $NetBSD: Makefile,v 1.5 2010/10/20 09:20:12 jmmv Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-sh
ATFFILE=	yes

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-sh

CPPFLAGS+=	-DHAVE_CONFIG_H

TESTS_SH=	atf-check_test
TESTS_SH+=	atf_check_test
TESTS_SH+=	config_test
TESTS_SH+=	integration_test
TESTS_SH+=	misc_helpers
TESTS_SH+=	normalize_test
TESTS_SH+=	tc_test
TESTS_SH+=	tp_test

.include <bsd.test.mk>
