#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=		rman

CPPFLAGS+=	-DVOLLIST='"1:2:3:4:5:6:7:8:9:o:l:n:p"' \
		-DMANTITLEPRINTF='"%s(%s) manual page"' \
		-DMANREFPRINTF='"%s.%s.html"' \
		-DPOLYGLOTMANVERSION='"3.0.8+XFree86"' \
		-DXFree86

.PATH:		${X11SRCDIR.xc}/extras/rman

.include <bsd.x11.mk>
.include <bsd.prog.mk>
