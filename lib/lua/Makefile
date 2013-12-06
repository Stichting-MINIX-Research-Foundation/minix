#	$NetBSD: Makefile,v 1.6 2013/11/13 09:49:08 mbalmer Exp $

.include <bsd.own.mk>

.if ${MKPIC} != "no"
# No support for shared libraries and pic code.
SUBDIR+=	gpio sqlite syslog
.endif

.include <bsd.subdir.mk>
