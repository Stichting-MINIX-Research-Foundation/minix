#	$NetBSD: Makefile,v 1.1 2010/07/25 21:42:08 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/if_loop

TESTS_C=	t_pr

LDADD+=		-lrumpnet_netinet -lrumpnet_net -lrumpnet
LDADD+=		-lrump -lrumpuser -lpthread

.include <bsd.test.mk>
