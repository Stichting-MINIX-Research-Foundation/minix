#	$NetBSD: libmesa7.mk,v 1.1 2015/01/05 03:45:31 mrg Exp $

# This copy for old MesaLib 7.x drivers only.

#
# Consumer of this Makefile should set MESA_SRC_MODULES.

INCLUDES.all=	mapi mesa mesa/main

# The source file lists derived from src/mesa/sources.mak and
# src/mapi/glapi/sources.mak.  Please keep the organization in line
# with those files.

# Main sources
PATHS.main=	mesa/main
INCLUDES.main=	glsl
SRCS.main= \
	api_exec_es1.c \
	api_exec_es2.c

SRCS.main+= \
	api_arrayelt.c \
	api_exec.c \
	api_loopback.c \
	api_noop.c \
	api_validate.c \
	accum.c \
	arbprogram.c \
	atifragshader.c \
	attrib.c \
	arrayobj.c \
	blend.c \
	bufferobj.c \
	buffers.c \
	clear.c \
	clip.c \
	colortab.c \
	condrender.c \
	context.c \
	convolve.c \
	cpuinfo.c \
	debug.c \
	depth.c \
	depthstencil.c \
	dlist.c \
	dlopen.c \
	drawpix.c \
	drawtex.c \
	enable.c \
	enums.c \
	MESAeval.c \
	execmem.c \
	extensions.c \
	fbobject.c \
	feedback.c \
	ffvertex_prog.c \
	fog.c \
	formats.c \
	framebuffer.c \
	get.c \
	getstring.c \
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
	nvprogram.c \
	pack.c \
	pbo.c \
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
	shaderapi.c \
	shaderobj.c \
	shared.c \
	state.c \
	stencil.c \
	syncobj.c \
	texcompress.c \
	texcompress_rgtc.c \
	texcompress_s3tc.c \
	texcompress_fxt1.c \
	texenv.c \
	texfetch.c \
	texformat.c \
	texgen.c \
	texgetimage.c \
	teximage.c \
	texobj.c \
	texpal.c \
	texparam.c \
	texstate.c \
	texstore.c \
	texturebarrier.c \
	transformfeedback.c \
	uniforms.c \
	varray.c \
	version.c \
	viewport.c \
	vtxfmt.c

SRCS.main+= \
	ff_fragment_shader.cpp

# XXX  avoid source name clashes with glx
.PATH:		${X11SRCDIR.MesaLib7}/src/mesa/main
BUILDSYMLINKS=	${X11SRCDIR.MesaLib7}/src/mesa/main/pixel.c MESApixel.c \
		${X11SRCDIR.MesaLib7}/src/mesa/main/pixelstore.c MESApixelstore.c \
		${X11SRCDIR.MesaLib7}/src/mesa/main/eval.c MESAeval.c

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

SRCS.math+= \
	m_xform.c

# Software raster sources
PATHS.swrast=		mesa/swrast
SRCS.swrast= \
	s_aaline.c \
	s_aatriangle.c \
	s_accum.c \
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
	s_readpix.c \
	s_span.c \
	s_stencil.c \
	s_texcombine.c \
	s_texfilter.c \
	s_texrender.c \
	s_triangle.c \
	s_zoom.c

# swrast_setup
PATHS.ss=	mesa/swrast_setup
SRCS.ss= \
	ss_context.c \
	ss_triangle.c 

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

# VBO sources
PATHS.vbo=	mesa/vbo
SRCS.vbo= \
	vbo_context.c \
	vbo_exec.c \
	vbo_exec_api.c \
	vbo_exec_array.c \
	vbo_exec_draw.c \
	vbo_exec_eval.c \
	vbo_rebase.c \
	vbo_split.c \
	vbo_split_copy.c \
	vbo_split_inplace.c \
	vbo_save.c \
	vbo_save_api.c \
	vbo_save_draw.c \
	vbo_save_loopback.c 

# statetracker

# Program sources
PATHS.program=		mesa/program
SRCS.program= \
	arbprogparse.c \
	hash_table.c \
	lex.yy.c \
	nvfragparse.c \
	nvvertparse.c \
	program.c \
	program_parse.tab.c \
	program_parse_extra.c \
	prog_cache.c \
	prog_execute.c \
	prog_instruction.c \
	prog_noise.c \
	prog_optimize.c \
	prog_parameter.c \
	prog_parameter_layout.c \
	prog_print.c \
	prog_statevars.c \
	prog_uniform.c \
	programopt.c \
	register_allocate.c \
	symbol_table.c

SRCS.program+= \
	ir_to_mesa.cpp \
	sampler.cpp

# Unused parts of mesa/sources.mak.
.if 0
ASM_C_SOURCES =	\
	x86/common_x86.c \
	x86/x86_xform.c \
	x86/3dnow.c \
	x86/sse.c \
	x86/rtasm/x86sse.c \
	sparc/sparc.c \
	ppc/common_ppc.c \
	x86-64/x86-64.c

X86_SOURCES =			\
	x86/common_x86_asm.S	\
	x86/x86_xform2.S	\
	x86/x86_xform3.S	\
	x86/x86_xform4.S	\
	x86/x86_cliptest.S	\
	x86/mmx_blend.S		\
	x86/3dnow_xform1.S	\
	x86/3dnow_xform2.S	\
	x86/3dnow_xform3.S	\
	x86/3dnow_xform4.S	\
	x86/3dnow_normal.S	\
	x86/sse_xform1.S	\
	x86/sse_xform2.S	\
	x86/sse_xform3.S	\
	x86/sse_xform4.S	\
	x86/sse_normal.S	\
	x86/read_rgba_span_x86.S

X86-64_SOURCES =		\
	x86-64/xform4.S

SPARC_SOURCES =			\
	sparc/clip.S		\
	sparc/norm.S		\
	sparc/xform.S
.endif

# Common driver sources
PATHS.common=	mesa/drivers/common
SRCS.common= \
	driverfuncs.c	\
	meta.c

# OSMesa driver sources
PATHS.osmesa=	mesa/drivers/osmesa
SRCS.osmesa= \
	osmesa.c

# GLAPI sources
PATHS.glapi=	mapi/glapi
SRCS.glapi = \
	glapi_dispatch.c \
	glapi_entrypoint.c \
	glapi_gentable.c \
	glapi_getproc.c \
	glapi_nop.c \
	glthread.c \
	glapi.c

# Unused parts of mapi/glapi/sources.mak.
.if 0
X86_API =		\
	glapi_x86.S

X86-64_API =		\
	glapi_x86-64.S

SPARC_API =		\
	glapi_sparc.S
.endif

.for _mod_ in ${MESA_SRC_MODULES}

SRCS+=	${SRCS.${_mod_}}

. for _path_ in ${PATHS.${_mod_}}
.PATH:	${X11SRCDIR.MesaLib7}/src/${_path_}
. endfor

. for _path_ in ${INCLUDES.${_mod_}}
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/src/${_path_}
. endfor

.endfor

.for _path_ in ${INCLUDES.all}
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/src/${_path_}
.endfor

LIBDPLIBS=	m	${NETBSDSRCDIR}/lib/libm

# build the shader headers
.include "../libglsl7.mk"

CPPFLAGS+=	-I.
CPPFLAGS+=	-I${X11SRCDIR.MesaLib7}/include

cleandir:     cleanmesa
cleanmesa: .PHONY
	-@if [ -d library ]; then rmdir library; fi
