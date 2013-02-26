# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:56 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/integration/helpers
# Even though we use bsd.test.mk here because some of the helpers are
# actually ATF-based test programs, we don't want to install an Atffile
# because they are not runnable on their own.
ATFFILE=	no

.PATH:		${SRCDIR}/integration/helpers

TESTS_CXX=	bogus_test_cases \
		config \
		expect_all_pass \
		expect_some_fail \
		interrupts \
		metadata \
		simple_all_pass \
		simple_some_fail

PROGS_CXX=			bad_test_program
BINDIR.bad_test_program=	${TESTSDIR}
MAN.bad_test_program=		# none

.include <bsd.test.mk>
