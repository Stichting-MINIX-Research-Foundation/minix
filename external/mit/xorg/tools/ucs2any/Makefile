#	$NetBSD: Makefile,v 1.2 2008/09/13 21:37:07 hubertf Exp $

NOMAN=		1

.include <bsd.own.mk>

HOSTPROG=	ucs2any
.PATH:		${X11SRCDIR.font-util}

HOST_CPPFLAGS= 	-DDARWIN_NO_CARBON

.include <bsd.x11.mk>
.include <bsd.hostprog.mk>
