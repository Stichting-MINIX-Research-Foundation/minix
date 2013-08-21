#	$NetBSD: Makefile,v 1.2 2010/05/22 02:40:40 mrg Exp $

.include <bsd.own.mk>

PROG=	dga

LDADD+=	-lXxf86dga -lXext -lX11
DPADD+=	${LIBXXF86DGA} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.xf86dga}

COPTS.dga.c+=	-Wno-error	# XXX xf86dga.h

.include <bsd.x11.mk>
.include <bsd.prog.mk>
