#	$NetBSD: Makefile,v 1.2 2013/05/31 05:24:49 mrg Exp $

.include <bsd.own.mk>

PROG=	fstobdf
SRCS=	chars.c fstobdf.c header.c props.c

LDADD+=	-lFS -lXext -lX11
DPADD+=	${LIBFS} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
