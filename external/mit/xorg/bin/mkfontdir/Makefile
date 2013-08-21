#	$NetBSD: Makefile,v 1.3 2013/05/31 05:42:14 mrg Exp $

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.mkfontdir}
.PATH:	${X11SRCDIR.mkfontdir}/man

MAN=	mkfontdir.1

SCRIPTS=	mkfontdir

mkfontdir: mkfontdir.in
	${TOOL_SED} -e "s#@bindir@#${X11BINDIR}#" < ${.ALLSRC} > ${.TARGET}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
