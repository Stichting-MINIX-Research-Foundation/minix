# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:57 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/format

.PATH:		${SRCDIR}/utils/format

TESTS_CXX=	exceptions_test \
		formatter_test

.include <bsd.test.mk>
