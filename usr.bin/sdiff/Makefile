#	$NetBSD: Makefile,v 1.3 2015/01/29 03:28:19 christos Exp $

WARNS?=	2	# XXX -Wcast-qual issues

PROG=	sdiff
SRCS=	common.c edit.c sdiff.c
CPPFLAGS+=-D_OPENBSD_SOURCE

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
