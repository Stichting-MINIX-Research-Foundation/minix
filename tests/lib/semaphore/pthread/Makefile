# $NetBSD: Makefile,v 1.2 2012/03/10 21:58:49 joerg Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/semaphore/pthread

TESTS_C=	t_sem_pth

CPPFLAGS+=	-I${.CURDIR}/..

LDADD+=		-lrump -lrumpuser -lpthread

WARNS=	4

.include <bsd.test.mk>
