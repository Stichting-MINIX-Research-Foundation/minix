# $NetBSD: Makefile,v 1.7 2014/02/12 04:08:32 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/atf/atf-c/detail

.PATH:		${SRCDIR}/atf-c/detail

PROGS=
.for prog in	process_helpers \
		version_helper
PROGS+=		${prog}
SRCS.${prog}=	${prog}.c
MAN.${prog}=	# defined
BINDIR.${prog}=	${TESTSDIR}
.endfor

TESTS_C=
.for test in	dynstr_test \
		env_test \
		fs_test \
		list_test \
		map_test \
		process_test \
		sanity_test \
		text_test \
		user_test
TESTS_C+=	${test}
SRCS.${test}=	${test}.c test_helpers.c
.endfor
.undef test

# Don't warn about functions which cannot be stack smash protected as
# there are a lot of them.
COPTS.dynstr_test.c=	-Wno-stack-protector

.include <bsd.test.mk>
