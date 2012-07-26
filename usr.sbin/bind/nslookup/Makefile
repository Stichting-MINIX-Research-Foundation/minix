#	$NetBSD: Makefile,v 1.7 2004/05/18 00:10:39 christos Exp $

.include <bsd.own.mk>

PROG=	nslookup
SRCS=	nslookup.c dighost.c
MAN=	nslookup.8
BINDIR=	/usr/bin

.include "${.CURDIR}/../Makefile.inc"

DIST=${IDIST}/bin/dig
CPPFLAGS+= -I${DIST}/include

.PATH: ${DIST}

.include <bsd.prog.mk>
