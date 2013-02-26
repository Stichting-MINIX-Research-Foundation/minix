# $NetBSD: Makefile,v 1.1 2013/02/25 00:20:09 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-atf-compat

.for test in atf-report_test atf-run_test atf2kyua_test
TESTS_SH+=	${test}
TESTS_SH_SRC_${test}=	${test}.tmp.sh
${test}.tmp.sh: ${test}.sh
	src=${test}.sh dst=${test}.tmp.sh; ${BUILD_SCRIPT}
CLEANFILES+=	${test}.tmp.sh ${test}.tmp.sh.new
.endfor

FILES=			helpers
FILESDIR_helpers=	${TESTSDIR}
realall: helpers
helpers: helpers.sh
	src=helpers.sh dst=helpers; ${BUILD_SCRIPT}
CLEANFILES+=	helpers helpers.new

.include <bsd.test.mk>
