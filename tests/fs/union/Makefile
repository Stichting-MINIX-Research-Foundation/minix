#	$NetBSD: Makefile,v 1.3 2011/01/12 21:13:26 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/union
WARNS=		4

TESTS_C=	t_pr

LDADD+=	-lrumpfs_union -lrumpvfs_layerfs -lrumpfs_ffs		# fs drivers
LDADD+=	-lrumpdev_disk -lrumpdev				# disk device
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
