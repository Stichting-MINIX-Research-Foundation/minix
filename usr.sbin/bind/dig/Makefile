#	$NetBSD: Makefile,v 1.4 2004/05/18 00:10:39 christos Exp $

.include <bsd.own.mk>

PROG=	dig
SRCS=	dig.c dighost.c
BINDIR=	/usr/bin

.include "${.CURDIR}/../Makefile.inc"

DIST=${IDIST}/bin/dig
CPPFLAGS+= -I${DIST}/include

.PATH: ${DIST}

.include <bsd.prog.mk>
