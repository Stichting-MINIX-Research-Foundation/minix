#	$NetBSD: Makefile,v 1.5 2015/01/07 22:24:03 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/md
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_md

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_mdserv

LDADD+=	-lrumpdev_md -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrumpkern_sysproxy -lrump
LDADD+=	-lrumpuser
LDADD+=	-lpthread

WARNS=	4
NOMAN=

.include <bsd.test.mk>
