# $NetBSD: Makefile,v 1.5 2014/02/12 04:08:32 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-c++/detail

.PATH:		${SRCDIR}/atf-c++/detail

PROGS_CXX=
.for prog in	version_helper
PROGS_CXX+=	${prog}
SRCS.${prog}=	${prog}.cpp
MAN.${prog}=	# defined
BINDIR.${prog}=	${TESTSDIR}
.endfor

TESTS_CXX=
.for test in	application_test \
		auto_array_test \
		env_test \
		exceptions_test \
		fs_test \
		process_test \
		sanity_test \
		text_test
TESTS_CXX+=	${test}
SRCS.${test}=	${test}.cpp test_helpers.cpp
.endfor
.undef test

.include <bsd.test.mk>
