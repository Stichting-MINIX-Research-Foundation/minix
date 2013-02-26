# $NetBSD: Makefile,v 1.3 2011/02/22 10:36:14 pooka Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/rump/rumpvfs

TESTS_C=	t_basic
TESTS_C+=	t_etfs
TESTS_C+=	t_p2kifs

LDADD=	-lrumpvfs -lrump -lrumpuser -lpthread

WARNS=	4

.include <bsd.test.mk>
