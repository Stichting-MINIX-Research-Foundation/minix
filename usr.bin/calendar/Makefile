#	$NetBSD: Makefile,v 1.14 2009/04/14 22:15:17 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

PROG=	calendar
.if ${MKSHARE} != "no"
FILESDIR=/usr/share/calendar
FILES!= echo ${.CURDIR}/calendars/calendar.*
.endif

.include <bsd.prog.mk>
