#	$NetBSD: Makefile,v 1.5 2014/12/18 06:24:28 mrg Exp $

NOMAN=	# defined

.include <bsd.own.mk>

.if ${MKPIC} == "no" || ${LDSTATIC:U} != ""
PROG_CXX=	glxinfo
.else
PROG=		glxinfo
.endif

SRCS+=	glinfo_common.c glxinfo.c

CPPFLAGS+=${X11FLAGS.THREADS} -DDO_GLU

#__MINIX: No pthread
LDADD+=	-lGL -lXext -lX11 -lm
DPADD+=	${LIBGL} ${LIBXEXT} ${LIBX11} ${LIBM}

.PATH:	${X11SRCDIR.mesa-demos}/src/xdemos

.include <bsd.x11.mk>
.include <bsd.prog.mk>
