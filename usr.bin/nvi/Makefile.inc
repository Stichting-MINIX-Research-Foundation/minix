#	$NetBSD: Makefile.inc,v 1.2 2008/07/13 03:44:41 christos Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
NOCLANGERROR=yes
NOGCCERROR=yes
.endif

DIST= ${NETBSDSRCDIR}/dist/nvi
.PATH: ${DIST}
