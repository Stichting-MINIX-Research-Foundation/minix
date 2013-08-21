# $NetBSD: Makefile,v 1.4 2011/08/11 23:15:37 joerg Exp $

MODULE=	tdfx

SRCS=	tdfx_context.c tdfx_dd.c tdfx_lock.c tdfx_pixels.c tdfx_render.c \
	tdfx_screen.c tdfx_span.c tdfx_state.c tdfx_tex.c tdfx_texman.c \
	tdfx_texstate.c tdfx_tris.c tdfx_vb.c

COPTS.tdfx_screen.c=  -Wno-error	# XXX const
CWARNFLAGS.clang+=	-Wno-constant-logical-operand \
			-Wno-tautological-compare

.include "${.CURDIR}/../dri.mk"
