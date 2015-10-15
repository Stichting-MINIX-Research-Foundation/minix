# $NetBSD: Makefile,v 1.8 2014/03/03 19:56:32 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-sh
ATFFILE=	yes
KYUAFILE=	yes

.PATH:		${SRCDIR}/atf-sh

TESTS_SH=	atf-check_test
TESTS_SH+=	atf_check_test
TESTS_SH+=	config_test
TESTS_SH+=	integration_test
TESTS_SH+=	misc_helpers
TESTS_SH+=	normalize_test
TESTS_SH+=	tc_test
TESTS_SH+=	tp_test

.include <bsd.test.mk>
