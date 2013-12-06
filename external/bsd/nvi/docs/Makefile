# $NetBSD: Makefile,v 1.1 2013/11/22 16:00:45 christos Exp $

SUBDIR= USD.doc info

.if defined(__MINIX)
# LSC: No groff for now...
SUBDIR= info
.endif # defined(__MINIX)
.include <bsd.subdir.mk>
