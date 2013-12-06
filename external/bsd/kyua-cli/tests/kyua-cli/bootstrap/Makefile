# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:55 jmmv Exp $

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/kyua-cli/bootstrap
ATFFILE=	no

.PATH:		${SRCDIR}/bootstrap

FILES=			testsuite
FILESDIR=		${TESTSDIR}
FILESMODE=		${BINMODE}

PROGS_CXX=		atf_helpers
BINDIR.atf_helpers=	${TESTSDIR}
MAN.atf_helpers=	# none
LDADD.atf_helpers=	-latf-c++ -latf-c
DPADD.atf_helpers=	${LIBATF_CXX} ${LIBATF_C}

PROGS_CXX+=		plain_helpers
BINDIR.plain_helpers=	${TESTSDIR}
MAN.plain_helpers=	# none

.include <bsd.files.mk>
.include <bsd.test.mk>
