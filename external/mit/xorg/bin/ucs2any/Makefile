#	$NetBSD: Makefile,v 1.3 2011/11/11 22:27:33 njoly Exp $

.include <bsd.own.mk>

PROG=	ucs2any

X11EXTRAMANDEFS+= 	-e 's,__mapfilesdir__,${LIBDIR}/X11/fonts/util,'

.PATH:	${X11SRCDIR.font-util} ${X11SRCDIR.font-util}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
