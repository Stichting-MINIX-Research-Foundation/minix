#	$NetBSD: Makefile,v 1.3 2015/07/22 07:58:00 mrg Exp $

.include <bsd.own.mk>

PROG=	fslsfonts

LDADD+=	-lFS
DPADD+=	${LIBFS}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include "../../xorg-pkg-ver.mk"

.include <bsd.x11.mk>
.include <bsd.prog.mk>
