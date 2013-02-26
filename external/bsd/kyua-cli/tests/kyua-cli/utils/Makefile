# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:56 jmmv Exp $

KYUA_LIBS=	utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/utils

.PATH:		${SRCDIR}/utils

TESTS_CXX=	auto_array_test \
		datetime_test \
		env_test \
		memory_test \
		optional_test \
		passwd_test \
		sanity_test \
		stream_test \
		units_test

TESTS_SUBDIRS+=	cmdline config format fs logging process signals sqlite text

.include <bsd.test.mk>
