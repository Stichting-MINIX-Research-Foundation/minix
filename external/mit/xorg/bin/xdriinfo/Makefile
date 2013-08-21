#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=	xdriinfo

SRCS=	xdriinfo.c

#LSC: No pthreads on MINIX!
LDADD+=	-lGL -lXext -lX11
DPADD+=	${LIBGL} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
