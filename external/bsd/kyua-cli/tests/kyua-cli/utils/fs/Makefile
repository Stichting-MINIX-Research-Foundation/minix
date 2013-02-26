# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:57 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/fs

.PATH:		${SRCDIR}/utils/fs

#LSC: Unsupported on MINIX
# operations_test

TESTS_CXX=	auto_cleaners_test \
		exceptions_test \
		lua_module_test \
		path_test

.include <bsd.test.mk>
