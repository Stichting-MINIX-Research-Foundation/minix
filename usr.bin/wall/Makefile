#	$NetBSD: Makefile,v 1.10 2007/05/28 12:06:32 tls Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

USE_FORT?= yes	# setuid
PROG=	wall
SRCS=	wall.c utmpentry.c term_chk.c
BINGRP=	tty
BINMODE=2555
DPADD+=${LIBUTIL}
LDADD+=-lutil

.PATH.c: ${NETBSDSRCDIR}/usr.bin/who ${NETBSDSRCDIR}/usr.bin/write
CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin/who -DSUPPORT_UTMPX -DSUPPORT_UTMP
CPPFLAGS+=-I${NETBSDSRCDIR}/usr.bin/write

.include <bsd.prog.mk>
