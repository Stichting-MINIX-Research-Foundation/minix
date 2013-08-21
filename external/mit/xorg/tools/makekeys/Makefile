#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:23 mrg Exp $

NOMAN=	1

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.X11}/src/util

HOSTPROG=	makekeys
HOST_CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}

.include <bsd.x11.mk>
.include <bsd.hostprog.mk>
