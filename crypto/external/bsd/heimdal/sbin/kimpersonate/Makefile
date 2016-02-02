# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH: ${HEIMDIST}/kuser

PROG=	kimpersonate
SRCS=	kimpersonate.c
MAN=	kimpersonate.8

LDADD+= -lkafs -lheimntlm
DPADD+= ${LIBKAFS} ${LIBHEIMNTLM}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
