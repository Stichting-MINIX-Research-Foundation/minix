#	$NetBSD: Makefile,v 1.8 2007/05/28 12:06:33 tls Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

USE_FORT?= yes	# setgid
PROG=	write
SRCS=	write.c utmpentry.c term_chk.c
BINMODE=2555
BINGRP=	tty

.PATH.c: ${NETBSDSRCDIR}/usr.bin/who
CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin/who -DSUPPORT_UTMPX -DSUPPORT_UTMP

.include <bsd.prog.mk>
