# $NetBSD: Makefile,v 1.6 2014/02/15 04:15:20 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/atf/test-programs
ATFFILE=	yes
KYUAFILE=	yes

.PATH:		${SRCDIR}/test-programs

TESTS_C=	c_helpers

TESTS_CXX=	cpp_helpers

TESTS_SH=	sh_helpers
.for t in config_test expect_test meta_data_test result_test srcdir_test
TESTS_SH+=		${t}
TESTS_SH_SRC_${t}=	common.sh ${t}.sh
.endfor

.include <bsd.test.mk>
