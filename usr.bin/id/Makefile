#	$NetBSD: Makefile,v 1.13 2011/08/16 10:37:21 christos Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	id
MAN=	id.1 groups.1 whoami.1
LINKS=   ${BINDIR}/id ${BINDIR}/groups
LINKS+=  ${BINDIR}/id ${BINDIR}/whoami

COPTS.id.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
