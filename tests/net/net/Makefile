# $NetBSD: Makefile,v 1.13 2015/10/07 05:24:41 ozaki-r Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/net

TESTS_C=	t_unix
TESTS_C+=	t_tcp
TESTS_C+=	t_udp
TESTS_C+=	t_pktinfo
.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_C+=	t_raw
TESTS_SH=	t_forwarding
TESTS_SH+=	t_ipaddress
TESTS_SH+=	t_ipv6_lifetime
.endif

LDADD.t_raw+=	-lrumpnet_local -lrumpnet_netinet -lrumpnet_net
LDADD.t_raw+=	-lrumpnet -lrumpvfs -lrump -lrumpuser -lrump -lpthread

.include <bsd.test.mk>
