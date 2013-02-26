#	$NetBSD: Makefile,v 1.4 2011/10/13 17:23:29 plunky Exp $
#

TESTSDIR=	${TESTSBASE}/fs/ptyfs
WARNS?=		4

TESTS_C=	t_ptyfs
TESTS_C+=	t_nullpts

LDADD+=	-lrumpfs_ptyfs						# fs drivers
LDADD+=	-lrumpkern_tty						# tty support
LDADD+=	-lrumpvfs -lrump -lrumpuser -lpthread			# base

LDADD.t_nullpts+= -lrumpfs_null -lrumpvfs_layerfs

.include <bsd.test.mk>
