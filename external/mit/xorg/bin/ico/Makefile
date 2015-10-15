#	$NetBSD: Makefile,v 1.2 2015/07/22 07:58:00 mrg Exp $

.include <bsd.own.mk>

PROG=	ico

#__MINIX: No Pthreads: -DMULTITHREAD
CPPFLAGS+= ${X11FLAGS.THREADS} ${X11FLAGS.EXTENSION}

#__MINIX: No pthreads
LDADD+=	-lXext -lX11 -lm
DPADD+=	${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.${PROG}}
.PATH:	${X11SRCDIR.${PROG}}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
