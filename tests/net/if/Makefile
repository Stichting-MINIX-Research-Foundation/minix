# $NetBSD: Makefile,v 1.1 2010/11/07 19:53:42 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/if

TESTS_C=	t_compat

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet -lrump
LDADD+=		-lrumpuser -lpthread

.include <bsd.test.mk>
