#	$NetBSD: Makefile,v 1.13 2010/11/12 17:35:34 pooka Exp $
#

.include <bsd.own.mk>

SUBDIR+=	h_dtfs

TESTSDIR=	${TESTSBASE}/fs/puffs

TESTS_C=	t_basic
TESTS_C+=	t_fuzz
TESTS_C+=	t_io

VFSTESTDIR != cd ${.CURDIR}/../common && ${PRINTOBJDIR}
LDADD+=-L${VFSTESTDIR} -lvfstest

LDADD+=		-lrumpdev_putter -lrumpdev
LDADD+=		-lrumpfs_syspuffs -lrumpvfs_fifofs
LDADD+=		-lrumpnet_local -lrumpnet_net -lrumpnet
LDADD+=		-lrumpvfs
LDADD+=		-lrump -lrumpuser -lpthread

# required by -DPUFFSDUMP
#LDADD+= 	-lpuffs -lutil

WARNS=2

.include <bsd.test.mk>
