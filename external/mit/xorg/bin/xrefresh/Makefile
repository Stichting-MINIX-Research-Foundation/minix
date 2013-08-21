#	$NetBSD: Makefile,v 1.2 2013/05/31 08:01:59 mrg Exp $

.include <bsd.own.mk>

PROG=	xrefresh

LDADD+=	-lXext -lX11
DPADD+=	${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
