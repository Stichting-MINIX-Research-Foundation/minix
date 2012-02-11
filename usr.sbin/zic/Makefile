#	$NetBSD: Makefile,v 1.9 2009/04/22 15:23:10 lukem Exp $

WARNS?=	2	# XXX -Wcast-qual in lib/libc/time

.include <bsd.own.mk>

PROG=	zic
SRCS=	zic.c scheck.c ialloc.c
MAN=	zic.8
CPPFLAGS+=-Dunix

.PATH: ${NETBSDSRCDIR}/lib/libc/time

.include <bsd.prog.mk>
