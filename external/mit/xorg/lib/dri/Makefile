# $NetBSD: Makefile,v 1.26 2015/01/05 18:51:10 gson Exp $

# Link the mesa_dri_drivers mega driver.

.include <bsd.own.mk>

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"

LIBISMODULE=	yes
LIBISCXX=     yes

SHLIB_MAJOR=	0

LIB=		mesa_dri_drivers
DRIDIR=		${X11USRLIBDIR}/modules/dri
DRIDEBUGDIR=	${DEBUGDIR}${X11USRLIBDIR}/modules/dri

#	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/${MODULE}/server \

CPPFLAGS+= \
	-I${X11SRCDIR.MesaLib}/src/egl/main \
	-I${X11SRCDIR.MesaLib}/src/egl/drivers/dri \
	-I${X11SRCDIR.MesaLib}/../src/mesa/drivers/dri/common \
	-I${DESTDIR}${X11INCDIR}/libdrm

#CPPFLAGS+=	-D_NETBSD_SOURCE -DPTHREADS
.if defined(__MINIX)
CPPFLAGS+=	-DHAVE_PTHREAD=1
LDADD+=		-lmthread
DPADD+=		 ${LIBMTHREAD}
CWARNFLAGS.clang+=	-Wno-\#warnings
#LSC: For clang 3.4
CWARNFLAGS.clang+=	-Wno-attributes
.endif # defined(__MINIX)


# We don't actually build this on non-x86 at all, currently.
# The following if statements are not effective since we only
# get here for x86
.if ${MACHINE_ARCH} == "alpha"
DRIVERS=	r200 radeon
.elif ${MACHINE} == "macppc" || ${MACHINE} == "ofppc"
DRIVERS=	r200 radeon
.elif ${MACHINE_ARCH} == "sparc64" || ${MACHINE_ARCH} == "sparc"
DRIVERS=	r200 radeon
.elif ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
DRIVERS=	i915 i965 r200 radeon
.elif ${MACHINE} == "prep" || ${MACHINE} == "bebox"
DRIVERS=	r200 radeon
.endif

DRI_SUBDIRS= ${DRIVERS}

DRI_SOURCES.i915 = \
	i830_context.c \
	i830_state.c \
	i830_texblend.c \
	i830_texstate.c \
	i830_vtbl.c \
	i915_tex_layout.c \
	i915_texstate.c \
	i915_context.c \
	i915_debug_fp.c \
	i915_fragprog.c \
	i915_program.c \
	i915_state.c \
	i915_vtbl.c \

I915_INTEL_FILES = \
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
	intel_tex.c \
	intel_pixel.c \
	intel_pixel_bitmap.c \
	intel_pixel_copy.c \
	intel_pixel_draw.c \
	intel_pixel_read.c \
	intel_buffers.c \
	intel_blit.c \
	intel_context.c \
	intel_screen.c \
	intel_state.c \
	intel_syncobj.c \
	intel_tris.c \
	intel_fbo.c

.for _f in ${I915_INTEL_FILES}
BUILDSYMLINKS+=		${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i915/${_f} i915_${_f}
DRI_SOURCES.i915+=	i915_${_f}
CPPFLAGS.i915_${_f}+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i915
.endfor

DRI_SOURCES.i965 = \
	brw_binding_tables.c \
	brw_blorp.cpp \
	brw_blorp_blit.cpp \
	brw_blorp_blit_eu.cpp \
	brw_cc.c \
	brw_cfg.cpp \
	brw_clear.c \
	brw_clip.c \
	brw_clip_line.c \
	brw_clip_point.c \
	brw_clip_state.c \
	brw_clip_tri.c \
	brw_clip_unfilled.c \
	brw_clip_util.c \
	brw_context.c \
	brw_cubemap_normalize.cpp \
	brw_curbe.c \
	brw_dead_control_flow.cpp \
	brw_device_info.c \
	brw_disasm.c \
	brw_draw.c \
	brw_draw_upload.c \
	brw_eu.c \
	brw_eu_compact.c \
	brw_eu_emit.c \
	brw_eu_util.c \
	brw_fs.cpp \
	brw_fs_channel_expressions.cpp \
	brw_fs_copy_propagation.cpp \
	brw_fs_cse.cpp \
	brw_fs_dead_code_eliminate.cpp \
	brw_fs_fp.cpp \
	brw_fs_generator.cpp \
	brw_fs_live_variables.cpp \
	brw_fs_peephole_predicated_break.cpp \
	brw_fs_reg_allocate.cpp \
	brw_fs_register_coalesce.cpp \
	brw_fs_saturate_propagation.cpp \
	brw_fs_sel_peephole.cpp \
	brw_fs_vector_splitting.cpp \
	brw_fs_visitor.cpp \
	brw_gs.c \
	brw_gs_emit.c \
	brw_gs_state.c \
	brw_gs_surface_state.c \
	brw_gs.c \
	brw_gs_emit.c \
	brw_gs_state.c \
	brw_gs_surface_state.c \
	brw_interpolation_map.c \
	brw_lower_texture_gradients.cpp \
	brw_lower_unnormalized_offset.cpp \
	brw_meta_updownsample.c \
	brw_meta_stencil_blit.c \
	brw_meta_util.c \
	brw_meta_fast_clear.c \
	brw_misc_state.c \
	brw_object_purgeable.c \
	brw_performance_monitor.c \
	brw_program.c \
	brw_primitive_restart.c \
	brw_queryobj.c \
	brw_reset.c \
	brw_sampler_state.c \
	brw_schedule_instructions.cpp \
	brw_sf.c \
	brw_sf_emit.c \
	brw_sf_state.c \
	brw_shader.cpp \
	brw_state_batch.c \
	brw_state_cache.c \
	brw_state_dump.c \
	brw_state_upload.c \
	brw_surface_formats.c \
	brw_tex.c \
	brw_tex_layout.c \
	brw_urb.c \
	brw_util.c \
	brw_vec4.cpp \
	brw_vec4_copy_propagation.cpp \
	brw_vec4_cse.cpp \
	brw_vec4_generator.cpp \
	brw_vec4_gs.c \
	brw_vec4_gs_visitor.cpp \
	brw_vec4_live_variables.cpp \
	brw_vec4_reg_allocate.cpp \
	brw_vec4_visitor.cpp \
	brw_vec4_vp.cpp \
	brw_vec4_vs_visitor.cpp \
	brw_vs.c \
	brw_vs_state.c \
	brw_vs_surface_state.c \
	brw_wm.c \
	brw_wm_iz.cpp \
	brw_wm_state.c \
	brw_wm_surface_state.c \
	gen6_blorp.cpp \
	gen6_cc.c \
	gen6_clip_state.c \
	gen6_depth_state.c \
	gen6_depthstencil.c \
	gen6_gs_state.c \
	gen6_multisample_state.c \
	gen6_queryobj.c \
	gen6_sampler_state.c \
	gen6_scissor_state.c \
	gen6_sf_state.c \
	gen6_sol.c \
	gen6_surface_state.c \
	gen6_urb.c \
	gen6_viewport_state.c \
	gen6_vs_state.c \
	gen6_wm_state.c \
	gen7_blorp.cpp \
	gen7_disable.c \
	gen7_gs_state.c \
	gen7_misc_state.c \
	gen7_sf_state.c \
	gen7_sol_state.c \
	gen7_urb.c \
	gen7_viewport_state.c \
	gen7_vs_state.c \
	gen7_wm_state.c \
	gen7_wm_surface_state.c \
	gen8_blend_state.c \
	gen8_depth_state.c \
	gen8_disable.c \
	gen8_draw_upload.c \
	gen8_gs_state.c \
	gen8_misc_state.c \
	gen8_multisample_state.c \
	gen8_sf_state.c \
	gen8_sol_state.c \
	gen8_surface_state.c \
	gen8_viewport_state.c \
	gen8_vs_state.c \
	gen8_wm_depth_stencil.c \
	gen8_ps_state.c

I965_INTEL_FILES = \
	intel_asm_annotation.c \
	intel_batchbuffer.c \
	intel_blit.c \
	intel_buffer_objects.c \
	intel_buffers.c \
	intel_copy_image.c \
	intel_debug.c \
	intel_extensions.c \
	intel_fbo.c \
	intel_mipmap_tree.c \
	intel_resolve_map.c \
	intel_screen.c \
	intel_pixel.c \
	intel_pixel_bitmap.c \
	intel_pixel_copy.c \
	intel_pixel_draw.c \
	intel_pixel_read.c \
	intel_state.c \
	intel_syncobj.c \
	intel_tex.c \
	intel_tex_copy.c \
	intel_tex_image.c \
	intel_tex_subimage.c \
	intel_tex_validate.c \
	intel_upload.c \

.for _f in ${I965_INTEL_FILES}
BUILDSYMLINKS+=		${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i965/${_f} i965_${_f}
DRI_SOURCES.i965+=	i965_${_f}
CPPFLAGS.i965_${_f} +=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/i965
.endfor

DRI_SOURCES.r200 = \
	r200_context.c \
	r200_ioctl.c \
	r200_state.c \
	r200_state_init.c \
	r200_cmdbuf.c \
	r200_tex.c \
	r200_texstate.c \
	r200_tcl.c \
	r200_swtcl.c \
	r200_maos.c \
	r200_sanity.c \
	r200_fragshader.c \
	r200_vertprog.c \
	r200_blit.c \

R200_RADEON_FILES= \
	radeon_buffer_objects.c \
	radeon_common_context.c \
	radeon_common.c \
	radeon_dma.c \
	radeon_debug.c \
	radeon_fbo.c \
	radeon_fog.c \
	radeon_mipmap_tree.c \
	radeon_pixel_read.c \
	radeon_queryobj.c \
	radeon_span.c \
	radeon_texture.c \
	radeon_tex_copy.c \
	radeon_tile.c \
	radeon_screen.c

.for _f in ${R200_RADEON_FILES}
BUILDSYMLINKS+=		${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon/${_f} r200_${_f}
DRI_SOURCES.r200+=	r200_${_f}
.endfor

.for _f in ${DRI_SOURCES.r200}
CPPFLAGS.${_f} +=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/r200/server \
			-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/r200 \
			-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon/server \
			-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon \
			-DRADEON_R200
.endfor

DRI_SOURCES.radeon = \
	radeon_buffer_objects.c \
	radeon_common_context.c \
	radeon_common.c \
	radeon_dma.c \
	radeon_debug.c \
	radeon_fbo.c \
	radeon_fog.c \
	radeon_mipmap_tree.c \
	radeon_pixel_read.c \
	radeon_queryobj.c \
	radeon_span.c \
	radeon_texture.c \
	radeon_tex_copy.c \
	radeon_tile.c \
	radeon_context.c \
	radeon_ioctl.c \
	radeon_screen.c \
	radeon_state.c \
	radeon_state_init.c \
	radeon_tex.c \
	radeon_texstate.c \
	radeon_tcl.c \
	radeon_swtcl.c \
	radeon_maos.c \
	radeon_sanity.c \
	radeon_blit.c

.for _f in ${DRI_SOURCES.radeon}
CPPFLAGS.${_f} +=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon/server \
			-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/radeon \
			-DRADEON_R100
.endfor

.for _d in ${DRI_SUBDIRS}
SRCS+=	${DRI_SOURCES.${_d}}
.PATH: ${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/${_d}
.endfor

LIBDPLIBS+=	expat		${NETBSDSRCDIR}/external/mit/expat/lib/libexpat
LIBDPLIBS+=	m		${NETBSDSRCDIR}/lib/libm
LIBDPLIBS+= 	glapi		${.CURDIR}/../libglapi
LIBDPLIBS+= 	drm		${.CURDIR}/../libdrm
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
LIBDPLIBS+= 	drm_intel	${.CURDIR}/../libdrm_intel
.endif
LIBDPLIBS+= 	drm_radeon	${.CURDIR}/../libdrm_radeon

MESA_SRC_MODULES=  main math math_xform vbo tnl swrast ss common asm_c program asm_s
.include "../libmesa.mk"
.include "../libglsl.mk"

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
SRCS+=	streaming-load-memcpy.c
CPPFLAGS.streaming-load-memcpy.c+= -msse4.1
.endif

.include "../driver.mk"

.for _d in ${DRIVERS}
SYMLINKS+= mesa_dri_drivers.so.${SHLIB_MAJOR} ${DRIDIR}/${_d}_dri.so.${SHLIB_MAJOR} 
SYMLINKS+= ${_d}_dri.so.${SHLIB_MAJOR} ${DRIDIR}/${_d}_dri.so
.if ${MKDEBUG} != "no"
SYMLINKS+= mesa_dri_drivers.so.${SHLIB_MAJOR}.debug ${DRIDEBUGDIR}/${_d}_dri.so.${SHLIB_MAJOR}.debug
.endif
.endfor

.endif

PKGCONFIG=	dri
PKGDIST.dri=	${X11SRCDIR.MesaLib}/src/mesa/drivers/dri
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.dri=	${MESA_VER}

# XXX remove these from bsd.x11.mk
PKGCONFIG_SED_FLAGS= \
	-e "s,@DRI_DRIVER_INSTALL_DIR@,${X11USRLIBDIR}/modules/dri,; \
	    s,@DRI_PC_REQ_PRIV@,,"

FILESDIR=	/etc
FILES=		drirc

.PATH:          ${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/common

.include <bsd.x11.mk>
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
LIBDIR=		${X11USRLIBDIR}/modules/dri

CWARNFLAGS.clang+=	-Wno-error=initializer-overrides -Wno-error=switch \
			-Wno-error=tautological-constant-out-of-range-compare

.include <bsd.lib.mk>
.else
.include <bsd.inc.mk>
.endif
# Don't re-build .c files when .y files change
.y.c:
