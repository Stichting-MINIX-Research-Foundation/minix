#	$NetBSD: Makefile,v 1.7 2010/11/22 21:05:29 mrg Exp $

.include <bsd.own.mk>

LIB=		glut

.PATH:	${X11SRCDIR.MesaGLUT}/src/glut/glx
SRCS=	\
	glut_8x13.c \
	glut_9x15.c \
	glut_bitmap.c \
	glut_bwidth.c \
	glut_cindex.c \
	glut_cmap.c \
	glut_cursor.c \
	glut_dials.c \
	glut_dstr.c \
	glut_event.c \
	glut_ext.c \
	glut_fcb.c \
	glut_fullscrn.c \
	glut_gamemode.c \
	glut_get.c \
	glut_glxext.c \
	glut_hel10.c \
	glut_hel12.c \
	glut_hel18.c \
	glut_init.c \
	glut_input.c \
	glut_joy.c \
	glut_key.c \
	glut_keyctrl.c \
	glut_keyup.c \
	glut_menu.c \
	glut_menu2.c \
	glut_mesa.c \
	glut_modifier.c \
	glut_mroman.c \
	glut_overlay.c \
	glut_ppm.c \
	glut_roman.c \
	glut_shapes.c \
	glut_space.c \
	glut_stroke.c \
	glut_swap.c \
	glut_swidth.c \
	glut_tablet.c \
	glut_teapot.c \
	glut_tr10.c \
	glut_tr24.c \
	glut_util.c \
	glut_vidresize.c \
	glut_warp.c \
	glut_win.c \
	glut_winmisc.c \
	layerutil.c

CPPFLAGS+=	-I${X11SRCDIR.MesaGLUT}/include -I${X11SRCDIR.MesaLib}/include
CFLAGS+=	-fno-strict-aliasing -fexceptions

.PATH:		${X11SRCDIR.MesaGLUT}/include/GL
INCS=	glut.h glutf90.h
INCSDIR=${X11INCDIR}/GL

LIBDPLIBS=	Xi	${.CURDIR}/../libXi \
		Xmu	${.CURDIR}/../libXmu \
		Xt	${.CURDIR}/../libXt \
		X11	${.CURDIR}/../libX11/dynamic \
		GL	${.CURDIR}/../libGL \
		GLU	${.CURDIR}/../libGLU

LDADD+=		-lm
DPADD+=		${LIBM}

PKGCONFIG=	glut
PKGDIST.glut=	${X11SRCDIR.MesaGLUT}/src/glut/glx
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.glut=	${MESA_VER}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
