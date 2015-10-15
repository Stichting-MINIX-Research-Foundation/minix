# $NetBSD: Makefile,v 1.3 2014/11/22 01:28:13 pettai Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH: ${HEIMDIST}/kuser

PROG= kdigest
MAN= kdigest.8

HEIMSRCS+= kdigest.c kdigest-commands.in

LDADD+= -lheimntlm -lsl -lutil -ledit -lterminfo
DPADD+= ${LIBHEIMNTLM} ${LIBSL} ${LIBUTIL} ${LIBEDIT} ${LIBTERMINFO}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
