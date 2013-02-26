#	$NetBSD: Makefile,v 1.3 2010/04/11 05:45:57 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/umapfs
WARNS=		4

TESTS_C=	t_basic

LDADD+=	-lrumpfs_tmpfs -lrumpfs_umap -lrumpvfs_layerfs		# fs drivers
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
