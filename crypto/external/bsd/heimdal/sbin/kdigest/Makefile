# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:19 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH: ${HEIMDIST}/kuser

PROG= kdigest
# no man-page yet
MAN=

HEIMSRCS+= kdigest.c kdigest-commands.in

LDADD+= -lheimntlm -lsl -lutil -ledit -lterminfo
DPADD+= ${LIBHEIMNTLM} ${LIBSL} ${LIBUTIL} ${LIBEDIT} ${LIBTERMINFO}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
