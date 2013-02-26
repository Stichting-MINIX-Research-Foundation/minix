# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:57 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/config

.PATH:		${SRCDIR}/utils/config

TESTS_CXX=	exceptions_test \
		keys_test \
		lua_module_test \
		nodes_test \
		parser_test \
		tree_test

.include <bsd.test.mk>
