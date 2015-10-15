#	$NetBSD: Makefile,v 1.1 2014/03/17 07:25:34 mrg Exp $

.include <bsd.own.mk>

LIB=	pthread-stubs
.PATH:	${X11SRCDIR.${LIB}}

SRCS=	stubs.c

CPPFLAGS+=	${X11FLAGS.THREADLIB} -I${X11SRCDIR.${LIB}}/../include

PKGDIST=	${LIB}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
