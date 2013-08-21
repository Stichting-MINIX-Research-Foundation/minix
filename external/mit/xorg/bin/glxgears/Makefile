#	$NetBSD: Makefile,v 1.2 2008/09/07 14:07:21 lukem Exp $

NOMAN=	# defined

.include <bsd.own.mk>

PROG=	glxgears

CPPFLAGS+=${X11FLAGS.THREADS}

#LSC: NO pthread on minix!
LDADD+=	-lGL -lXext -lX11 -lm
DPADD+=	${LIBGL} ${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.MesaDemos}/progs/xdemos

.include <bsd.x11.mk>
.include <bsd.prog.mk>
