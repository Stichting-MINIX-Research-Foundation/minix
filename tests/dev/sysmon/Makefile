#	$NetBSD: Makefile,v 1.2 2010/12/20 04:56:18 pgoyette Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/sysmon

TESTS_C=	t_swwdog

LDADD+=	-lrumpdev_sysmon -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser -lpthread

WARNS=	4

TESTS_SH=	t_swsensor

.include <bsd.test.mk>
