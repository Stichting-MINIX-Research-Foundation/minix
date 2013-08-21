#	$NetBSD: Makefile,v 1.1.1.1 2008/07/29 05:01:22 mrg Exp $

.include <bsd.own.mk>

PROG=	ico

CPPFLAGS+=-DMULTITHREAD ${X11FLAGS.THREADS} ${X11FLAGS.EXTENSION}

#LSC: No pthreads on MINIX!
LDADD+=	-lXext -lX11 -lm
DPADD+=	${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.${PROG}}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
