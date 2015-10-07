# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:16 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes	# cryptographic software

.PATH: ${HEIMDIST}/lib/gssapi

PROG= gsstool

HEIMSRCS=	gsstool.c gss-commands.in

MAN=

LDADD+= -lgssapi -lheimntlm
LDADD+= -lsl ${LIBVERS} 
LDADD+= -ledit -lterminfo
DPADD+= ${LIBGSSAPI} ${LIBHEIMNTLM}
DPADD+= ${LIBSL}
DPADD+= ${LIBEDIT} ${LIBTERMINFO}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
