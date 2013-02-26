#	$NetBSD: Makefile,v 1.3 2010/09/11 16:03:41 martin Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/scsipi

TESTS_C=	t_cd

LDADD+=	-lrumpdev_scsitest -lrumpdev_scsipi -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrump -lutil
LDADD+=	-lrumpuser -lpthread

WARNS=	4

.include <bsd.test.mk>
