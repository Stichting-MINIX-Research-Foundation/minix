# $NetBSD: Makefile,v 1.8 2011/08/11 23:15:37 joerg Exp $

MODULE=	radeon

CPPFLAGS+=	-DRADEON_COMMON=0 -DRADEON_R100
CPPFLAGS+=	-DHAVE_LIBDRM_RADEON

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon

SRCS=	\
	radeon_blit.c \
	radeon_bo_legacy.c \
	radeon_common_context.c \
	radeon_common.c \
	radeon_cs_legacy.c \
	radeon_dma.c \
	radeon_debug.c \
	radeon_fbo.c \
	radeon_lock.c \
	radeon_mipmap_tree.c \
	radeon_queryobj.c \
	radeon_span.c \
	radeon_texture.c \
	radeon_context.c \
	radeon_ioctl.c \
	radeon_screen.c \
	radeon_state.c \
	radeon_state_init.c \
	radeon_tex.c \
	radeon_tex_copy.c \
	radeon_texstate.c \
	radeon_tcl.c \
	radeon_swtcl.c \
	radeon_maos.c \
	radeon_sanity.c \

SRCS+= \
	radeon_cs_space_drm.c \
	radeon_bo.c \
	radeon_cs.c

LIBDPLIBS+=	drm_radeon	${.CURDIR}/../../libdrm_radeon

# XXX
.if ${MACHINE_ARCH} == "sparc64"
COPTS.radeon_tcl.c=	-Wno-error
.endif

COPTS.radeon_screen.c=  -Wno-error	# XXX const

CWARNFLAGS.clang+=	-Wno-constant-logical-operand -Wno-empty-body \
			-Wno-format

.include "${.CURDIR}/../dri.mk"
