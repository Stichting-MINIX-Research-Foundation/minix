# $NetBSD: Makefile,v 1.7 2012/07/11 22:40:16 jmmv Exp $

.include <bsd.own.mk>

TESTS_SUBDIRS=	detail

TESTSDIR=	${TESTSBASE}/atf/atf-c++
ATFFILE=	yes

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-c++
.PATH:		${SRCDIR}/atf-c++/detail

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libatf-c++

FILESDIR=	${TESTSDIR}
FILES=		macros_hpp_test.cpp
FILES+=		unused_test.cpp

TESTS_CXX=
.for test in	atf_c++_test \
		build_test \
		check_test \
		config_test \
		macros_test \
		tests_test \
		utils_test
TESTS_CXX+=	${test}
SRCS.${test}=	${test}.cpp test_helpers.cpp
.endfor
.undef test

TESTS_SH=	pkg_config_test

.include <bsd.test.mk>
