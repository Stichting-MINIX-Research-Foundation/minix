# $NetBSD: Makefile.inc,v 1.2 2008/09/19 23:00:49 joerg Exp $

.include <bsd.own.mk>

WARNS?=		4
BINDIR=		/sbin

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?=	-static
.endif

