#	$NetBSD: Makefile,v 1.1 2011/07/03 18:02:01 tron Exp $

.include <bsd.own.mk>

PROG=	lesskey

SRCS=	lesskey version.c  

LDADD+=	-ltermcap
DPADD+=	${LIBTERM}

.include "bsd.prog.mk"
