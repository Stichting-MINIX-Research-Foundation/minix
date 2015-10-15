# $NetBSD: Makefile,v 1.10 2014/02/12 04:08:32 jmmv Exp $

.include <bsd.init.mk>

TESTS_SUBDIRS=	detail

TESTSDIR=	${TESTSBASE}/atf/atf-c

.PATH:		${SRCDIR}/atf-c
.PATH:		${SRCDIR}/atf-c/detail

FILESDIR=	${TESTSDIR}
FILES=		macros_h_test.c
FILES+=		unused_test.c

TESTS_C=
.for test in	atf_c_test \
		build_test \
		check_test \
		config_test \
		error_test \
		macros_test \
		tc_test \
		tp_test \
		utils_test
TESTS_C+=	${test}
SRCS.${test}=	${test}.c test_helpers.c
.endfor
.undef test

TESTS_SH=	pkg_config_test

.include <bsd.test.mk>
