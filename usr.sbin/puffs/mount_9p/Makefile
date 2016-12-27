#	$NetBSD: Makefile,v 1.3 2009/04/22 15:23:06 lukem Exp $
#

PROG=	mount_9p
SRCS=	ninepuffs.c ninebuf.c nineproto.c fs.c node.c subr.c
LDADD+= -lpuffs -lutil

MAN=	mount_9p.8

.include <bsd.prog.mk>
