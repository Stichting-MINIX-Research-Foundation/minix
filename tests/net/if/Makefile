# $NetBSD: Makefile,v 1.5 2015/07/01 08:33:31 ozaki-r Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net/if

TESTS_C=	t_compat
TESTS_SH=	t_ifconf
TESTS_SH+=	t_ifconfig

PROGS=		ifconf
MAN.ifconf=	# empty
BINDIR.ifconf=	${TESTSDIR}

LDADD.t_compat=		-lrumpnet_shmif -lrumpnet_netinet -lrumpnet_net -lrumpnet -lrump
LDADD.t_compat+=	-lrumpuser -lrump -lpthread

.include <bsd.test.mk>
