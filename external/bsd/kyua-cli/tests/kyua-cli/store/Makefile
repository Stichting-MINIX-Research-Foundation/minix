# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:56 jmmv Exp $

KYUA_LIBS=	store engine utils

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/store

.PATH:		${SRCDIR}/store

TESTS_CXX=	backend_test \
		dbtypes_test \
		exceptions_test \
		metadata_test \
		schema_inttest \
		transaction_test

FILESDIR=	${TESTSDIR}
FILESMODE=	444
FILES=		schema_v1.sql \
		testdata_v1.sql \
		testdata_v2.sql

.include <bsd.test.mk>
