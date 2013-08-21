#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

CPPSCRIPTS=	xmkmf
CPPSCRIPTFLAGS=	-DCONFIGDIRSPEC=\"-I${X11LIBDIR}/config\"
SCRIPTS=	${CPPSCRIPTS}
MAN=		xmkmf.1

.PATH:	${X11SRCDIR.imake}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
