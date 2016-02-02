#	$NetBSD: libmesa.mk,v 1.1 2014/12/18 06:24:28 mrg Exp $
#
# Consumer of this Makefile should set MESA_SRC_MODULES.

INCLUDES.all=	mapi mesa mesa/main

# The source file lists derived from src/mesa/Makefile.sources.
# Please keep the organization in line with those files.

# Main sources
PATHS.main=	mesa/main
INCLUDES.main=	glsl
SRCS.main= \
	api_arrayelt.c \
	api_loopback.c \
	api_validate.c \
	accum.c \
	arbprogram.c \
	atifragshader.c \
	attrib.c \
	arrayobj.c \
	blend.c \
	blit.c \
	bufferobj.c \
	buffers.c \
	clear.c \
	clip.c \
	colortab.c \
	compute.c \
	condrender.c \
	context.c \
	convolve.c \
	copyimage.c \
	cpuinfo.c \
	debug.c \
	depth.c \
	dlist.c \
	drawpix.c \
	drawtex.c \
	enable.c \
	errors.c \
	MESAeval.c \
	execmem.c \
	extensions.c \
	fbobject.c \
	feedback.c \
	ffvertex_prog.c \
	ff_fragment_shader.cpp \
	fog.c \
	formatquery.c \
	formats.c \
	format_pack.c \
	format_unpack.c \
	format_utils.c \
	framebuffer.c \
	get.c \
	genmipmap.c \
	getstring.c \
	glformats.c \
	hash.c \
	hint.c \
	histogram.c \
	image.c \
	imports.c \
	light.c \
	lines.c \
	matrix.c \
	mipmap.c \
	mm.c \
	multisample.c \
	objectlabel.c \
	pack.c \
	pbo.c \
	performance_monitor.c \
	pipelineobj.c \
	MESApixel.c \
	MESApixelstore.c \
	pixeltransfer.c \
	points.c \
	polygon.c \
	queryobj.c \
	querymatrix.c \
	rastpos.c \
	readpix.c \
	remap.c \
	renderbuffer.c \
	samplerobj.c \
	scissor.c \
	set.c \
	shaderapi.c \
	shaderimage.c \
	shaderobj.c \
	shader_query.cpp \
	shared.c \
	state.c \
	stencil.c \
	syncobj.c \
	texcompress.c \
	texcompress_bptc.c \
	texcompress_cpal.c \
	texcompress_rgtc.c \
	texcompress_s3tc.c \
	texcompress_fxt1.c \
	texcompress_etc.c \
	texenv.c \
	texformat.c \
	texgen.c \
	texgetimage.c \
	teximage.c \
	texobj.c \
	texparam.c \
	texstate.c \
	texstorage.c \
	texstore.c \
	textureview.c \
	texturebarrier.c \
	transformfeedback.c \
	uniforms.c \
	uniform_query.cpp \
	varray.c \
	vdpau.c \
	version.c \
	viewport.c \
	vtxfmt.c \
	es1_conversion.c \

# Build files
.PATH:	${X11SRCDIR.MesaLib}/../src/mesa/main
SRCS.main+= \
	enums.c \
	api_exec.c \

# XXX  avoid source name clashes with glx
.PATH:		${X11SRCDIR.MesaLib}/src/mesa/main
BUILDSYMLINKS+=	${X11SRCDIR.MesaLib}/src/mesa/main/pixel.c MESApixel.c \
		${X11SRCDIR.MesaLib}/src/mesa/main/pixelstore.c MESApixelstore.c \
		${X11SRCDIR.MesaLib}/src/mesa/main/eval.c MESAeval.c

# Math sources
PATHS.math=	mesa/math
SRCS.math= \
	m_debug_clip.c \
	m_debug_norm.c \
	m_debug_xform.c \
	m_eval.c \
	m_matrix.c \
	m_translate.c \
	m_vector.c

PATHS.math_xform=	mesa/math
SRCS.math_xform= \
	m_xform.c


# VBO sources
PATHS.vbo=	mesa/vbo
INCLUDES.vbo=	gallium/auxiliary
SRCS.vbo= \
	vbo_context.c \
	vbo_exec.c \
	vbo_exec_api.c \
	vbo_exec_array.c \
	vbo_exec_draw.c \
	vbo_exec_eval.c \
	vbo_noop.c \
	vbo_primitive_restart.c \
	vbo_rebase.c \
	vbo_split.c \
	vbo_split_copy.c \
	vbo_split_inplace.c \
	vbo_save.c \
	vbo_save_api.c \
	vbo_save_draw.c \
	vbo_save_loopback.c

# TNL sources
PATHS.tnl=	mesa/tnl
SRCS.tnl= \
	t_context.c \
	t_pipeline.c \
	t_draw.c \
	t_rasterpos.c \
	t_vb_program.c \
	t_vb_render.c \
	t_vb_texgen.c \
	t_vb_texmat.c \
	t_vb_vertex.c \
	t_vb_fog.c \
	t_vb_light.c \
	t_vb_normals.c \
	t_vb_points.c \
	t_vp_build.c \
	t_vertex.c \
	t_vertex_sse.c \
	t_vertex_generic.c


# Software raster sources
PATHS.swrast=		mesa/swrast
SRCS.swrast= \
	s_aaline.c \
	s_aatriangle.c \
	s_alpha.c \
	s_atifragshader.c \
	s_bitmap.c \
	s_blend.c \
	s_blit.c \
	s_clear.c \
	s_copypix.c \
	s_context.c \
	s_depth.c \
	s_drawpix.c \
	s_feedback.c \
	s_fog.c \
	s_fragprog.c \
	s_lines.c \
	s_logic.c \
	s_masking.c \
	s_points.c \
	s_renderbuffer.c \
	s_span.c \
	s_stencil.c \
	s_texcombine.c \
	s_texfetch.c \
	s_texfilter.c \
	s_texrender.c \
	s_texture.c \
	s_triangle.c \
	s_zoom.c


# swrast_setup
PATHS.ss=	mesa/swrast_setup
SRCS.ss= \
	ss_context.c \
	ss_triangle.c 


# Common driver sources
PATHS.common=	mesa/drivers/common
SRCS.common= \
	driverfuncs.c   \
	meta_blit.c     \
	meta_copy_image.c       \
	meta_generate_mipmap.c  \
	meta.c


# ASM C driver sources
PATHS.asm_c=	mesa/x86 mesa/x86/rtasm mesa/sparc mesa/x86-64
SRCS.asm_c= \
	common_x86.c \
	x86_xform.c \
	3dnow.c \
	sse.c \
	x86sse.c \
	sparc.c \
	x86-64.c


# ASM assembler driver sources
PATHS.asm_s=	mesa/x86 mesa/x86/rtasm mesa/sparc mesa/x86-64
.if ${MACHINE} == "amd64"
SRCS.asm_s= \
	xform4.S
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/../src/arch/x86_64
.elif ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
SRCS.asm_s= \
	sparc_clip.S \
	norm.S \
	xform.S
.elif ${MACHINE} == "i386"
SRCS.asm_s= \
	common_x86_asm.S \
	x86_xform2.S \
	x86_xform3.S \
	x86_xform4.S \
	x86_cliptest.S \
	mmx_blend.S \
	3dnow_xform1.S \
	3dnow_xform2.S \
	3dnow_xform3.S \
	3dnow_xform4.S \
	3dnow_normal.S \
	sse_xform1.S \
	sse_xform2.S \
	sse_xform3.S \
	sse_xform4.S \
	sse_normal.S \
	read_rgba_span_x86.S
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/../src/arch/i386
.endif


# State tracker sources
PATHS.state_tracker=	mesa/state_tracker
INCLUDES.state_tracker=	glsl
SRCS.state_tracker= \
	st_atom.c \
	st_atom_array.c \
	st_atom_blend.c \
	st_atom_clip.c \
	st_atom_constbuf.c \
	st_atom_depth.c \
	st_atom_framebuffer.c \
	st_atom_msaa.c \
	st_atom_pixeltransfer.c \
	st_atom_sampler.c \
	st_atom_scissor.c \
	st_atom_shader.c \
	st_atom_rasterizer.c \
	st_atom_stipple.c \
	st_atom_texture.c \
	st_atom_viewport.c \
	st_cb_bitmap.c \
	st_cb_blit.c \
	st_cb_bufferobjects.c \
	st_cb_clear.c \
	st_cb_condrender.c \
	st_cb_flush.c \
	st_cb_drawpixels.c \
	st_cb_drawtex.c \
	st_cb_eglimage.c \
	st_cb_fbo.c \
	st_cb_feedback.c \
	st_cb_msaa.c \
	st_cb_program.c \
	st_cb_queryobj.c \
	st_cb_rasterpos.c \
	st_cb_readpixels.c \
	st_cb_syncobj.c \
	st_cb_strings.c \
	st_cb_texture.c \
	st_cb_texturebarrier.c \
	st_cb_viewport.c \
	st_cb_xformfb.c \
	st_context.c \
	st_debug.c \
	st_draw.c \
	st_draw_feedback.c \
	st_extensions.c \
	st_format.c \
	st_gen_mipmap.c \
	st_glsl_to_tgsi.cpp \
	st_manager.c \
	st_mesa_to_tgsi.c \
	st_program.c \
	st_texture.c \
	st_vdpau.c


# Program sources
PATHS.program=	mesa/program
INCLUDES.program=	glsl
SRCS.program= \
	arbprogparse.c \
	prog_hash_table.c \
	ir_to_mesa.cpp \
	program.c \
	program_parse_extra.c \
	prog_cache.c \
	prog_execute.c \
	prog_instruction.c \
	prog_noise.c \
	prog_optimize.c \
	prog_opt_constant_fold.c \
	prog_parameter.c \
	prog_parameter_layout.c \
	prog_print.c \
	prog_statevars.c \
	programopt.c \
	register_allocate.c \
	sampler.cpp \
	string_to_uint_map.cpp \
	symbol_table.c \
	program_lexer.l

# Generated
.PATH:	${X11SRCDIR.MesaLib}/../src/mesa/program
SRCS.program+= \
	program_parse.tab.c


# Run throught all the modules and setup the SRCS and CPPFLAGS etc.
.for _mod_ in ${MESA_SRC_MODULES}

SRCS+=	${SRCS.${_mod_}}

. for _path_ in ${PATHS.${_mod_}}
.PATH:	${X11SRCDIR.MesaLib}/src/${_path_}
. endfor

. for _path_ in ${INCLUDES.${_mod_}}
.  for _s in ${SRCS.${_mod_}}
CPPFLAGS.${_s}+=	-I${X11SRCDIR.MesaLib}/src/${_path_}
.  endfor
. endfor

.endfor

.for _path_ in ${INCLUDES.all}
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/${_path_}
.endfor

CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/include
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/main
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mapi
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/gallium/include
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/../src/mapi/glapi
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/../src/mesa
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/../src/mesa/main
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mesa/drivers/dri/common

CPPFLAGS+=	\
	-DPACKAGE_NAME=\"Mesa\" \
	-DPACKAGE_TARNAME=\"mesa\" \
	-DPACKAGE_VERSION=\"10.3.5\" \
	-DPACKAGE_STRING=\"Mesa\ 10.3.5\" \
	-DPACKAGE_BUGREPORT=\"https://bugs.freedesktop.org/enter_bug.cgi\?product=Mesa\" \
	-DPACKAGE_URL=\"\" \
	-DPACKAGE=\"mesa\" \
	-DVERSION=\"10.3.5\"

#__MINIX: No Pthreads: -DHAVE_PTHREAD=1
CPPFLAGS+=	\
	-DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 \
	-DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 \
	-DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 \
	-DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DHAVE___BUILTIN_BSWAP32=1 \
	-DHAVE___BUILTIN_BSWAP64=1 -DHAVE_DLADDR=1 -DHAVE_CLOCK_GETTIME=1 \
	-DHAVE_POSIX_MEMALIGN -DHAVE_DLOPEN

.include "../asm.mk"

CPPFLAGS+=	\
	-DHAVE_LIBDRM -DGLX_USE_DRM -DGLX_INDIRECT_RENDERING -DGLX_DIRECT_RENDERING -DHAVE_ALIAS -DMESA_EGL_NO_X11_HEADERS

CPPFLAGS+=	\
	-DUSE_EXTERNAL_DXTN_LIB=1 \
	-DYYTEXT_POINTER=1

CFLAGS+=	-fvisibility=hidden -fno-strict-aliasing -fno-builtin-memcmp
