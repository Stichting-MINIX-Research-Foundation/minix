# $NetBSD: Makefile,v 1.3 2011/08/11 23:15:38 joerg Exp $

MODULE=	unichrome

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/unichrome

SRCS=	via_context.c via_fb.c via_tex.c via_ioctl.c via_memcpy.c \
	via_render.c via_screen.c via_span.c via_state.c via_texcombine.c \
	via_tris.c 

CWARNFLAGS.clang+=	-Wno-constant-logical-operand

.include "${.CURDIR}/../dri.mk"
