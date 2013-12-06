# $NetBSD: Makefile,v 1.15 2013/07/19 04:00:40 kefren Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net

TESTS_SUBDIRS=		fdpass net route sys
.if (${MKRUMP} != "no")
TESTS_SUBDIRS+=		bpf bpfilter carp icmp if if_loop mpls npf
.endif

.include <bsd.test.mk>
