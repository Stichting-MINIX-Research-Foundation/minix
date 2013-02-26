#	$NetBSD: Makefile,v 1.5 2011/11/08 01:52:05 joerg Exp $

.include <bsd.own.mk>

.if ${MKPIC} != "no"
# No support for shared libraries and pic code.
SUBDIR+=	gpio sqlite
.endif

.include <bsd.subdir.mk>
