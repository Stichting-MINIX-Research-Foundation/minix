#	$NetBSD: Makefile,v 1.2 2012/03/22 23:46:27 joerg Exp $

.include <bsd.own.mk>

PROG=	xvinfo

LDADD+=	-lXv -lXext -lX11
DPADD+=	${LIBXV} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.${PROG}}

CWARNFLAGS.clang+=	-Wno-dangling-else

.include <bsd.x11.mk>
.include <bsd.prog.mk>
