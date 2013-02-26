# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:58 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/text

.PATH:		${SRCDIR}/utils/text

TESTS_CXX=	exceptions_test \
		operations_test \
		table_test \
		templates_test

.include <bsd.test.mk>
