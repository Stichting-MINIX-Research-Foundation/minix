#	$NetBSD: Makefile,v 1.2 2012/06/05 00:38:45 christos Exp $

.include <bsd.own.mk>

PROG=	nsupdate
MAN=	nsupdate.1
BINDIR=	/usr/bin

.include "${.CURDIR}/../Makefile.inc"

DIST=${IDIST}/bin/nsupdate

.PATH: ${DIST}
DPADD+=${LIBEDIT} ${LIBTERMINFO}
LDADD+=-ledit -lterminfo

.include <bsd.prog.mk>
