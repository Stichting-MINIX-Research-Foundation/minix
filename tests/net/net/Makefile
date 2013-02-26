# $NetBSD: Makefile,v 1.6 2013/07/03 14:01:29 nakayama Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/net

TESTS_C=	t_unix
TESTS_C+=	t_udp
TESTS_C+=	t_pktinfo
.if (${MKRUMP} != "no")
TESTS_C+=	t_raw
.endif

LDADD.t_raw+=		-lrumpnet_local -lrumpnet_netinet -lrumpnet_net
LDADD.t_raw+=		-lrumpnet -lrumpvfs -lrump -lrumpuser -lpthread

.include <bsd.test.mk>
