# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:56 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils/cmdline

.PATH:		${SRCDIR}/utils/cmdline

TESTS_CXX=	base_command_test \
		commands_map_test \
		exceptions_test \
		globals_test \
		options_test \
		parser_test \
		ui_test

.include <bsd.test.mk>
