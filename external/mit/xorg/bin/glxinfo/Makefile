#	$NetBSD: Makefile,v 1.3 2013/09/12 17:14:20 joerg Exp $

NOMAN=	# defined

.include <bsd.own.mk>

.if ${MKPIC} == "no" || ${LDSTATIC:U} != ""
PROG_CXX=	glxinfo
.else
PROG=		glxinfo
.endif

CPPFLAGS+=${X11FLAGS.THREADS} -DDO_GLU

#LSC: No pthread on MINIX!
LDADD+=	-lGLU -lGL -lXext -lX11 -lm
DPADD+=	${LIBGLU} ${LIBGL} ${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.MesaDemos}/progs/xdemos

.include <bsd.x11.mk>
.include <bsd.prog.mk>
