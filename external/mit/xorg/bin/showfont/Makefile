#	$NetBSD: Makefile,v 1.1 2015/07/23 07:34:38 mrg Exp $

.include <bsd.own.mk>

PROG=	showfont

LDADD+=	-lFS
DPADD+=	${LIBFS}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
