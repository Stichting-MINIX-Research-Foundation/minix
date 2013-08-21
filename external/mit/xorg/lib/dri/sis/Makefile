# $NetBSD: Makefile,v 1.3 2011/08/11 23:15:37 joerg Exp $

MODULE=	sis

SRCS=	sis6326_state.c sis6326_clear.c sis_alloc.c sis_clear.c sis_context.c \
	sis_dd.c sis_fog.c sis_lock.c sis_screen.c sis_span.c sis_state.c \
	sis_stencil.c sis_tex.c sis_texstate.c sis_tris.c

CWARNFLAGS.clang+=	-Wno-constant-logical-operand

.include "${.CURDIR}/../dri.mk"
