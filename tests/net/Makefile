# $NetBSD: Makefile,v 1.14 2013/07/03 14:01:29 nakayama Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/net

TESTS_SUBDIRS=		fdpass net route sys
.if (${MKRUMP} != "no")
TESTS_SUBDIRS+=		bpf bpfilter carp icmp if if_loop npf
.endif

.include <bsd.test.mk>
