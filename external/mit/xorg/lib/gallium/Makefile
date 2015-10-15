# $NetBSD: Makefile,v 1.7 2015/09/23 05:28:14 mrg Exp $

# Link the gallium mega driver.

LIBISMODULE=	yes
LIBISCXX=     yes

.include <bsd.own.mk>

SHLIB_MAJOR=    0

LIB=		gallium_dri
DRIDIR=		${X11USRLIBDIR}/modules/dri
DRIDEBUGDIR=	${DEBUGDIR}${X11USRLIBDIR}/modules/dri

DRIVERS=	kms_swrast swrast
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
DRIVERS+=	r600
.endif

CPPFLAGS+= \
	-I${DESTDIR}${X11INCDIR}/libdrm

# cargo culted.
#CPPFLAGS+= -D_NETBSD_SOURCE -DPTHREADS

GALLIUM_SUBDIRS= \
	auxiliary \
	auxiliary/cso_cache \
	auxiliary/draw \
	auxiliary/hud \
	auxiliary/indices \
	auxiliary/os \
	auxiliary/pipebuffer \
	auxiliary/postprocess \
	auxiliary/rbug \
	auxiliary/rtasm \
	auxiliary/tgsi \
	auxiliary/translate \
	auxiliary/util \
	auxiliary/vl \
	drivers/galahad \
	drivers/identity \
	drivers/noop \
	drivers/trace \
	drivers/rbug \
	drivers/radeon \
	drivers/r600 \
	drivers/r600/sb \
	drivers/softpipe \
	winsys/radeon/drm \
	winsys/sw/null \
	winsys/sw/dri \
	winsys/sw/kms-dri \
	state_trackers/dri

GALLIUM_SOURCES.auxiliary/cso_cache= \
	cso_cache.c \
	cso_context.c \
	cso_hash.c
GALLIUM_SOURCES.auxiliary/draw= \
	draw_context.c \
	draw_fs.c \
	draw_gs.c \
	draw_pipe.c \
	draw_pipe_aaline.c \
	draw_pipe_aapoint.c \
	draw_pipe_clip.c \
	draw_pipe_cull.c \
	draw_pipe_flatshade.c \
	draw_pipe_offset.c \
	draw_pipe_pstipple.c \
	draw_pipe_stipple.c \
	draw_pipe_twoside.c \
	draw_pipe_unfilled.c \
	draw_pipe_util.c \
	draw_pipe_validate.c \
	draw_pipe_vbuf.c \
	draw_pipe_wide_line.c \
	draw_pipe_wide_point.c \
	draw_prim_assembler.c \
	draw_pt.c \
	draw_pt_emit.c \
	draw_pt_fetch.c \
	draw_pt_fetch_emit.c \
	draw_pt_fetch_shade_emit.c \
	draw_pt_fetch_shade_pipeline.c \
	draw_pt_post_vs.c \
	draw_pt_so_emit.c \
	draw_pt_util.c \
	draw_pt_vsplit.c \
	draw_vertex.c \
	draw_vs.c \
	draw_vs_exec.c \
	draw_vs_variant.c
GALLIUM_SOURCES.auxiliary/hud= \
	font.c \
	hud_context.c \
	hud_cpu.c \
	hud_fps.c \
	hud_driver_query.c
GALLIUM_SOURCES.auxiliary/indices= \
	u_primconvert.c
GALLIUM_SOURCES.auxiliary/os= \
	os_misc.c \
	os_process.c \
	os_time.c
GALLIUM_SOURCES.auxiliary/pipebuffer= \
	pb_buffer_fenced.c \
	pb_buffer_malloc.c \
	pb_bufmgr_alt.c \
	pb_bufmgr_cache.c \
	pb_bufmgr_debug.c \
	pb_bufmgr_mm.c \
	pb_bufmgr_ondemand.c \
	pb_bufmgr_pool.c \
	pb_bufmgr_slab.c \
	pb_validate.c
GALLIUM_SOURCES.auxiliary/postprocess= \
	pp_celshade.c \
	pp_colors.c \
	pp_init.c \
	pp_mlaa.c \
	pp_run.c \
	pp_program.c
GALLIUM_SOURCES.auxiliary/rbug= \
	rbug_connection.c \
	rbug_context.c \
	rbug_core.c \
	rbug_demarshal.c \
	rbug_texture.c \
	rbug_shader.c
GALLIUM_SOURCES.auxiliary/rtasm= \
	rtasm_cpu.c \
	rtasm_execmem.c \
	rtasm_x86sse.c
GALLIUM_SOURCES.auxiliary/tgsi= \
	tgsi_build.c \
	tgsi_dump.c \
	tgsi_exec.c \
	tgsi_info.c \
	tgsi_iterate.c \
	tgsi_parse.c \
	tgsi_sanity.c \
	tgsi_scan.c \
	tgsi_strings.c \
	tgsi_text.c \
	tgsi_transform.c \
	tgsi_ureg.c \
	tgsi_util.c
GALLIUM_SOURCES.auxiliary/translate= \
	translate.c \
	translate_cache.c \
	translate_generic.c \
	translate_sse.c
GALLIUM_SOURCES.auxiliary/util= \
	u_debug.c \
	u_debug_describe.c \
	u_debug_flush.c \
	u_debug_memory.c \
	u_debug_refcnt.c \
	u_debug_stack.c \
	u_debug_symbol.c \
	u_dump_defines.c \
	u_dump_state.c \
	u_bitmask.c \
	u_blit.c \
	u_blitter.c \
	u_cache.c \
	u_caps.c \
	u_cpu_detect.c \
	u_dl.c \
	u_draw.c \
	u_draw_quad.c \
	u_format.c \
	u_format_other.c \
	u_format_latc.c \
	u_format_s3tc.c \
	u_format_rgtc.c \
	u_format_etc.c \
	u_format_bptc.c \
	u_format_tests.c \
	u_format_yuv.c \
	u_format_zs.c \
	u_framebuffer.c \
	u_gen_mipmap.c \
	u_handle_table.c \
	u_hash.c \
	u_hash_table.c \
	u_helpers.c \
	u_index_modify.c \
	u_keymap.c \
	u_linear.c \
	u_linkage.c \
	u_network.c \
	u_math.c \
	u_mm.c \
	u_pstipple.c \
	u_ringbuffer.c \
	u_sampler.c \
	u_simple_shaders.c \
	u_slab.c \
	u_snprintf.c \
	u_staging.c \
	u_suballoc.c \
	u_surface.c \
	u_surfaces.c \
	u_texture.c \
	u_tile.c \
	u_transfer.c \
	u_resource.c \
	u_upload_mgr.c \
	u_vbuf.c
GALLIUM_SOURCES.auxiliary/vl= \
	vl_csc.c \
	vl_compositor.c \
	vl_matrix_filter.c \
	vl_median_filter.c \
	vl_decoder.c \
	vl_mpeg12_decoder.c \
	vl_mpeg12_bitstream.c \
	vl_zscan.c \
	vl_idct.c \
	vl_mc.c \
	vl_vertex_buffers.c \
	vl_video_buffer.c \
	vl_deint_filter.c

# Generated
.PATH: ${X11SRCDIR.MesaLib}/../src/gallium/auxiliary
GALLIUM_SOURCES.auxiliary= \
	u_indices_gen.c \
	u_unfilled_gen.c \
	u_format_table.c
.for _f in ${GALLIUM_SOURCES.auxiliary}
CPPFLAGS.${_f} +=	-I${X11SRCDIR.MesaLib}/src/gallium/auxiliary/util
.endfor

GALLIUM_SOURCES.drivers/galahad= \
	glhd_objects.c \
	glhd_context.c \
	glhd_screen.c

GALLIUM_SOURCES.drivers/identity = \
        id_objects.c \
        id_context.c \
        id_screen.c

GALLIUM_SOURCES.drivers/noop = \
        noop_pipe.c \
        noop_state.c

GALLIUM_SOURCES.drivers/trace = \
        tr_context.c \
        tr_dump.c \
        tr_dump_state.c \
        tr_screen.c \
        tr_texture.c

GALLIUM_SOURCES.drivers/rbug = \
        DRIVERrbug_core.c \
        DRIVERrbug_context.c \
        rbug_objects.c \
        rbug_screen.c
# Conflicts with auxiliary/rbug/ files
BUILDSYMLINKS+=	${X11SRCDIR.MesaLib}/src/gallium/drivers/rbug/rbug_core.c    DRIVERrbug_core.c
BUILDSYMLINKS+=	${X11SRCDIR.MesaLib}/src/gallium/drivers/rbug/rbug_context.c DRIVERrbug_context.c
CPPFLAGS.DRIVERrbug_core.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/auxiliary
CPPFLAGS.DRIVERrbug_core.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/drivers/rbug
CPPFLAGS.DRIVERrbug_core.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/drivers
CPPFLAGS.DRIVERrbug_context.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/auxiliary
CPPFLAGS.DRIVERrbug_context.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/drivers/rbug
CPPFLAGS.DRIVERrbug_context.c+=	-I${X11SRCDIR.MesaLib}/src/gallium/drivers

GALLIUM_SOURCES.drivers/radeon = \
        cayman_msaa.c \
        r600_buffer_common.c \
        r600_pipe_common.c \
        r600_query.c \
        r600_streamout.c \
        r600_texture.c \
        radeon_video.c \
        radeon_uvd.c \
        radeon_vce.c \
        radeon_vce_40_2_2.c
#LLVM_C_FILES := \
#        radeon_elf_util.c \
#        radeon_setup_tgsi_llvm.c \
#        radeon_llvm_emit.c \
#        radeon_llvm_util.c

GALLIUM_SOURCES.drivers/r600 = \
        r600_asm.c \
        r600_blit.c \
        r600_hw_context.c \
        r600_isa.c \
        r600_pipe.c \
        r600_shader.c \
        r600_state.c \
        r700_asm.c \
        evergreen_hw_context.c \
        evergreen_state.c \
        eg_asm.c \
        r600_state_common.c \
        evergreen_compute.c \
        compute_memory_pool.c \
        r600_uvd.c
GALLIUM_SOURCES.drivers/r600/sb = \
        sb_bc_builder.cpp \
        sb_bc_decoder.cpp \
        sb_bc_dump.cpp \
        sb_bc_finalize.cpp \
        sb_bc_parser.cpp \
        sb_context.cpp \
        sb_core.cpp \
        sb_dce_cleanup.cpp \
        sb_def_use.cpp \
        sb_dump.cpp \
        sb_expr.cpp \
        sb_gcm.cpp \
        sb_gvn.cpp \
        sb_if_conversion.cpp \
        sb_ir.cpp \
        sb_liveness.cpp \
        sb_pass.cpp \
        sb_peephole.cpp \
        sb_psi_ops.cpp \
        sb_ra_checker.cpp \
        sb_ra_coalesce.cpp \
        sb_ra_init.cpp \
        sb_sched.cpp \
        sb_shader.cpp \
        sb_ssa_builder.cpp \
        sb_valtable.cpp
#LLVM_C_SOURCES = r600_llvm.c

.for _f in ${GALLIUM_SOURCES.drivers/r600} ${GALLIUM_SOURCES.drivers/r600/sb}
CPPFLAGS.${_f} +=	-I${X11SRCDIR.MesaLib}/src/gallium/drivers/r600
.endfor



GALLIUM_SOURCES.winsys/radeon/drm = \
        radeon_drm_bo.c \
        radeon_drm_cs.c \
        radeon_drm_cs_dump.c \
        radeon_drm_winsys.c

GALLIUM_SOURCES.drivers/softpipe = \
        sp_fs_exec.c \
        sp_clear.c \
        sp_fence.c \
        sp_flush.c \
        sp_query.c \
        sp_context.c \
        sp_draw_arrays.c \
        sp_prim_vbuf.c \
        sp_quad_pipe.c \
        sp_quad_stipple.c \
        sp_quad_depth_test.c \
        sp_quad_fs.c \
        sp_quad_blend.c \
        sp_screen.c \
        sp_setup.c \
        sp_state_blend.c \
        sp_state_clip.c \
        sp_state_derived.c \
        sp_state_sampler.c \
        sp_state_shader.c \
        sp_state_so.c \
        sp_state_rasterizer.c \
        sp_state_surface.c \
        sp_state_vertex.c \
        sp_texture.c \
        sp_tex_sample.c \
        sp_tex_tile_cache.c \
        sp_tile_cache.c \
        sp_surface.c

GALLIUM_SOURCES.winsys/sw/null = \
	null_sw_winsys.c

GALLIUM_SOURCES.winsys/sw/dri = \
	dri_sw_winsys.c

GALLIUM_SOURCES.winsys/sw/kms-dri = \
	kms_dri_sw_winsys.c

GALLIUM_SOURCES.state_trackers/dri = \
        dri_context.c \
        dri_drawable.c \
        dri_query_renderer.c \
        dri_screen.c \
	drisw.c \
	dri2.c
.for _f in ${GALLIUM_SOURCES.state_trackers/dri}
CPPFLAGS.${_f} +=	-I${X11SRCDIR.MesaLib}/src/gallium/state_trackers/dri/common
.endfor

# missing
CPPFLAGS+=	\
	-DGALLIUM_SOFTPIPE \
	-DGALLIUM_STATIC_TARGETS=1 \
	-DMESA_EGL_NO_X11_HEADERS \

.for _d in ${GALLIUM_SUBDIRS}
SRCS+=	${GALLIUM_SOURCES.${_d}}
.PATH: ${X11SRCDIR.MesaLib}/src/gallium/${_d}
.endfor

#. if defined(${GALLIUM_SOURCES.${_d}})
#SRCS+=	${GALLIUM_SOURCES.${_d}}
#. endif

.include "../libloader.mk"

LIBDPLIBS+=	m		${.CURDIR}/../../../../../lib/libm
LIBDPLIBS+= 	drm		${.CURDIR}/../libdrm
LIBDPLIBS+= 	drm_radeon	${.CURDIR}/../libdrm_radeon
LIBDPLIBS+= 	glapi		${.CURDIR}/../libglapi
LIBDPLIBS+=	expat		${.CURDIR}/../../../../../external/mit/expat/lib/libexpat

LDFLAGS+=	-Wl,--version-script=${X11SRCDIR.MesaLib}/src/gallium/targets/dri/dri.sym

##  build mesagallium parts
MESA_SRC_MODULES=  main math vbo state_tracker program asm_s
.include "../libmesa.mk"
.include "../libglsl.mk"

# Special addition for just gallium; it misses the rest of asm_c files.
.PATH: ${X11SRCDIR.MesaLib}/src/mesa/x86
SRCS+=	common_x86.c

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
SRCS+=	streaming-load-memcpy.c
CFLAGS.streaming-load-memcpy.c+= -msse4.1
.endif

.include "../driver.mk"


.PATH: ${X11SRCDIR.MesaLib}/src/gallium/targets/dri
SRCS+=	target.c

CPPFLAGS+= \
	-I${X11SRCDIR.MesaLib}/src/gallium/include \
	-I${X11SRCDIR.MesaLib}/src/gallium/auxiliary \
	-I${X11SRCDIR.MesaLib}/src/gallium/drivers \
	-I${X11SRCDIR.MesaLib}/../src/mesa/drivers/dri/common

CPPFLAGS.target.c += \
	-DDRI_TARGET \
	-DGALLIUM_GALAHAD \
	-DGALLIUM_NOOP \
	-DGALLIUM_RBUG \
	-DGALLIUM_TRACE \
	-DGALLIUM_R200 \
	-DGALLIUM_R600 \
	-DGALLIUM_SOFTPIPE \
	-I${X11SRCDIR.MesaLib}/src/gallium/state_trackers/dri \
	-I${X11SRCDIR.MesaLib}/src/loader \
	-I${X11SRCDIR.MesaLib}/src/gallium/winsys

CWARNFLAGS.clang+=	-Wno-error=constant-conversion \
			-Wno-error=tautological-constant-out-of-range-compare \
			-Wno-error=pointer-sign \
			-Wno-error=switch \
			-Wno-error=absolute-value \
			-Wno-error=tautological-compare \
			-Wno-error=static-in-inline

.include <bsd.x11.mk>
LIBDIR=		${X11USRLIBDIR}/modules/dri

.for _d in ${DRIVERS}
SYMLINKS+= gallium_dri.so ${DRIDIR}/${_d}_dri.so
SYMLINKS+= gallium_dri.so.${SHLIB_MAJOR} ${DRIDIR}/${_d}_dri.so.${SHLIB_MAJOR} 
.if ${MKDEBUG} != "no"
SYMLINKS+= gallium_dri.so.${SHLIB_MAJOR}.debug ${DRIDEBUGDIR}/${_d}_dri.so.${SHLIB_MAJOR}.debug
.endif
.endfor

.include <bsd.lib.mk>
# Don't regenerate c files
.y.c:
