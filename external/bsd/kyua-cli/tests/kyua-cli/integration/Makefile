# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:56 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/integration

.PATH:		${SRCDIR}/integration

TESTS_SH+=			cmd_about_test
TESTS_SH_SRC_cmd_about_test=	utils.sh cmd_about_test.tmp.sh
CLEANFILES+=			cmd_about_test.tmp.sh cmd_about_test.tmp.sh.tmp
cmd_about_test.tmp.sh: cmd_about_test.sh
	sed -e 's,__KYUA_DOCDIR__,${KYUA_DOCDIR},g' \
	    <${.ALLSRC} >${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

.for test in \
	cmd_config_test \
	cmd_db_exec_test \
	cmd_db_migrate_test \
	cmd_debug_test \
	cmd_help_test \
	cmd_list_test \
	cmd_report_html_test \
	cmd_report_test \
	cmd_test_test \
	global_test
TESTS_SH+=		${test}
TESTS_SH_SRC_${test}=	utils.sh ${test}.sh
.endfor

SUBDIR+=	helpers

.include <bsd.test.mk>
