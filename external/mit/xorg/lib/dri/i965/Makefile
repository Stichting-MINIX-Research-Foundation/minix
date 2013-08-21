# $NetBSD: Makefile,v 1.14 2012/03/22 23:46:27 joerg Exp $

MODULE=	i965

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i965 \
		-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel \
		-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel/server

DRI_EXTRA_PATHS=	${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel \
			${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/intel/server
SRCS=	\
	intel_batchbuffer.c \
	intel_blit.c \
	intel_buffer_objects.c \
	intel_buffers.c \
	intel_clear.c \
	intel_context.c \
	intel_decode.c \
	intel_extensions.c \
	intel_fbo.c \
	intel_mipmap_tree.c \
	intel_regions.c \
	intel_screen.c \
	intel_span.c \
	intel_pixel.c \
	intel_pixel_bitmap.c \
	intel_pixel_copy.c \
	intel_pixel_draw.c \
	intel_pixel_read.c \
	intel_state.c \
	intel_syncobj.c \
	intel_tex.c \
	intel_tex_copy.c \
	intel_tex_format.c \
	intel_tex_image.c \
	intel_tex_layout.c \
	intel_tex_subimage.c \
	intel_tex_validate.c \
	brw_cc.c \
	brw_clip.c \
	brw_clip_line.c \
	brw_clip_point.c \
	brw_clip_state.c \
	brw_clip_tri.c \
	brw_clip_unfilled.c \
	brw_clip_util.c \
	brw_context.c \
	brw_curbe.c \
	brw_disasm.c \
	brw_draw.c \
	brw_draw_upload.c \
	brw_eu.c \
	brw_eu_debug.c \
	brw_eu_emit.c \
	brw_eu_util.c \
	brw_fallback.c \
	brw_gs.c \
	brw_gs_emit.c \
	brw_gs_state.c \
	brw_misc_state.c \
	brw_program.c \
	brw_queryobj.c \
	brw_sf.c \
	brw_sf_emit.c \
	brw_sf_state.c \
	brw_state_batch.c \
	brw_state_cache.c \
	brw_state_dump.c \
	brw_state_upload.c \
	brw_tex.c \
	brw_tex_layout.c \
	brw_urb.c \
	brw_util.c \
	brw_vs.c \
	brw_vs_constval.c \
	brw_vs_emit.c \
	brw_vs_state.c \
	brw_vs_surface_state.c \
	brw_vtbl.c \
	brw_wm.c \
	brw_wm_debug.c \
	brw_wm_emit.c \
	brw_wm_fp.c \
	brw_wm_iz.c \
	brw_wm_glsl.c \
	brw_wm_pass0.c \
	brw_wm_pass1.c \
	brw_wm_pass2.c \
	brw_wm_sampler_state.c \
	brw_wm_state.c \
	brw_wm_surface_state.c \
	gen6_cc.c \
	gen6_clip_state.c \
	gen6_depthstencil.c \
	gen6_gs_state.c \
	gen6_sampler_state.c \
	gen6_scissor_state.c \
	gen6_sf_state.c \
	gen6_urb.c \
	gen6_viewport_state.c \
	gen6_vs_state.c \
	gen6_wm_state.c

COPTS.brw_draw_upload.c=	-Wno-error
CWARNFLAGS.clang+=	-Wno-format -Wno-switch

LIBDPLIBS+=	drm_intel	${.CURDIR}/../../libdrm_intel

COPTS.intel_context.c+=	-Wno-error	# XXX
COPTS.intel_tex.c+=	-Wno-error	# XXX

.include "${.CURDIR}/../dri.mk"
