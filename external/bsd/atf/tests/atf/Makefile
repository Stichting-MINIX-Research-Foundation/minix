# $NetBSD: Makefile,v 1.7 2013/02/25 00:22:11 jmmv Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/atf

TESTS_SUBDIRS=	atf-c \
		atf-c++ \
		atf-sh \
		test-programs

.if !(${MKKYUA} != "no")
TESTS_SUBDIRS+=	atf-config \
		atf-report \
		atf-run
.endif

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}

.include <bsd.test.mk>
