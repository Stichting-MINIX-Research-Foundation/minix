# $NetBSD: Makefile,v 1.2 2013/02/15 17:08:35 jmmv Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-c++/detail
ATFFILE=	yes

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-c++/detail

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libatf-c++

TESTS_CXX=
.for test in	application_test \
		auto_array_test \
		env_test \
		exceptions_test \
		expand_test \
		fs_test \
		parser_test \
		process_test \
		sanity_test \
		text_test \
		ui_test
TESTS_CXX+=	${test}
SRCS.${test}=	${test}.cpp test_helpers.cpp
.endfor
.undef test

.include <bsd.test.mk>
