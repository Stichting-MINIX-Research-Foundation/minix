#	$NetBSD: Makefile,v 1.3 2010/04/10 21:33:01 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/nullfs
WARNS=		4

TESTS_C=	t_basic

LDADD+=	-lrumpfs_tmpfs -lrumpfs_null -lrumpvfs_layerfs		# fs drivers
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
