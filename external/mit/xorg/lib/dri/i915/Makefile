# $NetBSD: Makefile,v 1.11 2011/08/11 23:15:36 joerg Exp $

MODULE=	i915

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i965 \
		-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel \
		-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel/server

DRI_EXTRA_PATHS=	${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel
SRCS=	\
	i830_context.c \
	i830_state.c \
	i830_texblend.c \
	i830_texstate.c \
	i830_vtbl.c \
	intel_render.c \
	intel_regions.c \
	intel_buffer_objects.c \
	intel_batchbuffer.c \
	intel_clear.c \
	intel_extensions.c \
	intel_mipmap_tree.c \
	intel_tex_layout.c \
	intel_tex_image.c \
	intel_tex_subimage.c \
	intel_tex_copy.c \
	intel_tex_validate.c \
	intel_tex_format.c \
	intel_tex.c \
	intel_pixel.c \
	intel_pixel_bitmap.c \
	intel_pixel_copy.c \
	intel_pixel_draw.c \
	intel_pixel_read.c \
	intel_buffers.c \
	intel_blit.c \
	i915_tex_layout.c \
	i915_texstate.c \
	i915_context.c \
	i915_debug.c \
	i915_debug_fp.c \
	i915_fragprog.c \
	i915_program.c \
	i915_state.c \
	i915_vtbl.c \
	intel_context.c \
	intel_decode.c \
	intel_screen.c \
	intel_span.c \
	intel_state.c \
	intel_syncobj.c \
	intel_tris.c \
	intel_fbo.c

CPPFLAGS+=	 -DI915 -DDRM_VBLANK_FLIP=DRM_VBLANK_FLIP

LIBDPLIBS+=	drm_intel	${.CURDIR}/../../libdrm_intel

COPTS.intel_context.c+=	-Wno-error	# XXX
COPTS.intel_tex.c+=	-Wno-error	# XXX
CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-format \
			-Wno-constant-logical-operand

.include "${.CURDIR}/../dri.mk"
