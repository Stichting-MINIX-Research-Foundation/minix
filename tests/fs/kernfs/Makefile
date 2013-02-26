#	$NetBSD: Makefile,v 1.3 2010/06/08 10:43:40 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/kernfs
WARNS=		4

TESTS_C=	t_basic

.include <bsd.own.mk>

LDADD+=	-lrumpfs_kernfs						# fs driver
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
