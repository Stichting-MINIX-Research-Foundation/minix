# $NetBSD: Makefile,v 1.1 2012/11/05 00:34:28 alnsn Exp $

TESTSDIR=	${TESTSBASE}/lib/libsljit

TESTS_SH=	t_sljit

.include <bsd.own.mk>
.include <../../../external/bsd/sljit/Makefile.inc>

.PATH:		${SLJITDIST}/test_src

BINDIR=		${TESTSDIR}

PROG=		h_sljit
MAN=		# defined
SRCS=		sljitMain.c sljitTest.c

WARNS=		3

LDADD+=		-L${LIBSLJITDIR} -lsljit
DPADD+=		${LIBSLJITDIR}/libsljit.a

.include <bsd.test.mk>
