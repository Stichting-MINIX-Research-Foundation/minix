#	$NetBSD: Makefile,v 1.6 2010/07/20 14:42:03 pooka Exp $
#

.include <bsd.own.mk>

MOUNTNAME=	nfs

SRCS+=		getnfsargs.c
LDADD+= 	-lrumpfs_nfs -lrumpnet -lrumpnet_sockin

ISRUMP=		# gives me nightmares
#CPPFLAGS+=	-DINET6 # for getnfsargs?

.include <bsd.prog.mk>
