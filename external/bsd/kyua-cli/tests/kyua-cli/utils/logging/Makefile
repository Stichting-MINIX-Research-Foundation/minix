# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:57 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/logging

.PATH:		${SRCDIR}/utils/logging

TESTS_CXX=	macros_test \
		operations_test

.include <bsd.test.mk>
