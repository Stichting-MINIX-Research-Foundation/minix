# $NetBSD: Makefile,v 1.2 2015/06/22 19:06:05 christos Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/db

TESTS_SH+=	t_db

TESTS_C+=	t_db_hash_seq

BINDIR=		${TESTSDIR}
MKMAN=		no

PROGS+=		h_db

FILESDIR=	${TESTSDIR}

FILES+=		README

.include <bsd.test.mk>
