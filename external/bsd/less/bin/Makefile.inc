# $NetBSD: Makefile.inc,v 1.1 2011/07/03 18:02:01 tron Exp $

.include "../Makefile.inc"

BINDIR=		/usr/bin

CPPFLAGS+=	-I${LESS_DISTDIR} -DBINDIR=\"${BINDIR}\" -DSYSDIR=\"/etc\"
