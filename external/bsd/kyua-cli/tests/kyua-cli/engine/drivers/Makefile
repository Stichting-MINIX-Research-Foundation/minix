# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:55 jmmv Exp $

KYUA_LIBS=	engine store engine utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/engine/drivers

.PATH:		${SRCDIR}/engine/drivers

TESTS_CXX=	debug_test_test \
		list_tests_test \
		run_tests_test \
		scan_action_test

PROGS_CXX=			list_tests_helpers
BINDIR.list_tests_helpers=	${TESTSDIR}
MAN.list_tests_helpers=		# none
LDADD.list_tests_helpers=	-latf-c++ -latf-c
DPADD.list_tests_helpers=	${LIBATF_CXX} ${LIBATF_C}

.include <bsd.test.mk>
