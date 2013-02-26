# $NetBSD: Makefile,v 1.1 2012/08/14 19:09:15 alnsn Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/bpfilter

TESTS_C=	t_bpfilter

LDADD+=		-lrumpnet_shmif
LDADD+=		-lrumpdev_bpf -lrumpdev -lrumpnet_netinet -lrumpnet_net
LDADD+=		-lrumpnet -lrumpvfs -lrump -lrumpuser -lpthread

.include <bsd.test.mk>
