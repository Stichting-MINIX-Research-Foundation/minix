# $NetBSD: Makefile,v 1.6 2015/09/14 05:22:56 ozaki-r Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/icmp

TESTS_C=	t_forward
TESTS_C+=	t_ping

TESTS_SH=	t_ping2 t_icmp_redirect t_icmp6_redirect

LDADD+=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet
LDADD+=		-lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
