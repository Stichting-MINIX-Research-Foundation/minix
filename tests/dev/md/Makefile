#	$NetBSD: Makefile,v 1.3 2012/12/02 18:39:53 pgoyette Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/dev/md
FILESDIR=	${TESTSDIR}

TESTS_SH=	t_md

BINDIR=		${TESTSDIR}
MKMAN=no

PROGS=		h_mdserv

LDADD+=	-lrumpdev_md -lrumpdev_disk -lrumpdev -lrumpvfs
LDADD+=	-lrump
LDADD+=	-lrumpuser
LDADD+=	-lpthread

WARNS=	4
NOMAN=

.include <bsd.test.mk>
