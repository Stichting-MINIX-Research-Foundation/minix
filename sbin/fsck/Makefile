#	$NetBSD: Makefile,v 1.18 2009/06/05 21:52:31 haad Exp $

PROG=	fsck
SRCS=	fsck.c fsutil.c preen.c
MAN=	fsck.8

LDADD+=-lutil
DPADD+=${LIBUTIL}

LDADD+=-lprop
DPADD+=${LIBPROP}

.include <bsd.prog.mk>
