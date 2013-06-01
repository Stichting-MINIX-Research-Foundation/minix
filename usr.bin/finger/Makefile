#	$NetBSD: Makefile,v 1.11 2007/05/28 12:06:26 tls Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

USE_FORT?= yes	# network client

PROG=	finger
SRCS=	finger.c lprint.c net.c sprint.c util.c utmpentry.c

.PATH.c: ${NETBSDSRCDIR}/usr.bin/who
CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin/who -DSUPPORT_UTMPX -DSUPPORT_UTMP

.if (${USE_INET6} != "no")
CPPFLAGS+=-DINET6
.endif

.include <bsd.prog.mk>
