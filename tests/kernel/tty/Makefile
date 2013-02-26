# $NetBSD: Makefile,v 1.1 2010/06/28 19:04:00 pooka Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/kernel/tty

TESTS_C=	t_pr

LDADD+=		-lrumpkern_tty -lrumpvfs -lrump -lrumpuser -lpthread

.include <bsd.test.mk>
