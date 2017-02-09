#	$NetBSD: Makefile,v 1.4 2009/04/22 15:23:07 lukem Exp $

PROG=	mount_psshfs
SRCS=	psshfs.c fs.c node.c psbuf.c subr.c
LDADD+= -lpuffs -lutil

MAN=	mount_psshfs.8

.include <bsd.prog.mk>
