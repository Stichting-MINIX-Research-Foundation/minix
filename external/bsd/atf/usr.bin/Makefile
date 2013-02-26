# $NetBSD: Makefile,v 1.4 2013/02/25 00:22:11 jmmv Exp $

.include <bsd.init.mk>

SUBDIR=		atf-sh
.if !(${MKKYUA} != "no")
SUBDIR+=	atf-config atf-report atf-run atf-version
.endif

.include <bsd.subdir.mk>
