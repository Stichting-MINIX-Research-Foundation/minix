#	$NetBSD: Makefile,v 1.1 2010/08/10 21:55:28 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/carp

TESTS_C=	t_basic

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net
LDADD+=		-lrumpnet -lrump -lrumpuser -lpthread

.include <bsd.test.mk>
