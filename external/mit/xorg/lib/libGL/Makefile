#	$NetBSD: Makefile,v 1.18 2012/03/22 23:46:27 joerg Exp $

.include <bsd.own.mk>

LIB=		GL

SUBDIR=		internal

# glx
.PATH:		${X11SRCDIR.MesaLib}/src/glx
SRCS.glx=	glcontextmodes.c clientattrib.c compsize.c eval.c glxcmds.c \
		glxext.c glxextensions.c indirect.c indirect_init.c \
		indirect_size.c indirect_window_pos.c \
		indirect_transpose_matrix.c indirect_vertex_array.c \
		indirect_vertex_program.c pixel.c pixelstore.c render2.c \
		renderpix.c single2.c singlepix.c vertarr.c xfont.c \
		glx_pbuffer.c glx_query.c \
		glxcurrent.c indirect_texture_compression.c

.PATH:		${X11SRCDIR.MesaLib}/src/mesa/glapi
SRCS.glx+=	glapi.c glapi_dispatch.c glapi_getproc.c glapi_nop.c glthread.c

# XXX see TODO
#.if ${MACHINE_ARCH} == "i386"
#.PATH:		${X11SRCDIR.MesaLib}/src/mesa/x86
#SRCS.glx+=	glapi_x86.S
#CPPFLAGS+=	-DUSE_X86_ASM -I${X11SRCDIR.MesaLib}/src/mesa/x86
## -DUSE_X86_ASM $(MMX_DEFS) $(3DNOW_DEFS) $(SSE_DEFS)
#.endif


.if ${X11DRI} != "no"
# dri
SRCS.dri=	XF86dri.c dri_glx.c drisw_glx.c dri_common.c \
		glxhash.c dri2_glx.c dri2.c
.for fn in ${SRCS.dri}
CPPFLAGS.${fn}=-DDEFAULT_DRIVER_DIR=\"${X11USRLIBDIR}/modules/dri\"
.endfor
.endif

MESA_SRC_MODULES=  main math vbo tnl shader swrast slang glapi common \
                        osmesa ss

.include "../libOSMesa/libmesa.mk"


.PATH:		${X11SRCDIR.MesaLib}/src/glw
.PATH:		${X11SRCDIR.MesaLib}/include/GL
.PATH:		${X11SRCDIR.MesaLib}/src/mesa/drivers/x11
INCS=	GLwDrawA.h GLwDrawAP.h GLwMDrawA.h GLwMDrawAP.h gl.h gl_mangle.h \
	glext.h glx.h glx_mangle.h glxext.h xmesa.h xmesa_x.h \
	xmesa_xf86.h
INCSDIR=${X11INCDIR}/GL

SRCS+=	${SRCS.dri} ${SRCS.glx}

.include "../../tools/glsl/Makefile.glslsrcs"

SRCS+=	${SRCS.pp} ${SRCS.cl}

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/GL \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/libdrm \
		-I${X11SRCDIR.MesaLib}/include \
		-I${X11SRCDIR.MesaLib}/include/GL/internal \
		-I${X11SRCDIR.MesaLib}/src/mesa/main \
		-I${X11SRCDIR.MesaLib}/src/mesa/glapi \
		-I${X11SRCDIR.MesaLib}/src/mesa \
		${X11FLAGS.THREADLIB}

#		-I${X11SRCDIR.dri2proto}/src/mesa \

.if !defined(__MINIX)
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden -pthread
CPPFLAGS+=	-DUSE_SSE_SYSCTL_DETECTION \
		-D__GLX_ALIGN64 \
		-DMESA_EXECMEM_MMAP \
		-DEXEC_HEAP_SIZE=10485760 \
		-D_NETBSD_SOURCE \
		-DUSE_NATIVE_LIBM_FUNCS \
		-DPTHREADS \
		-DUSE_EXTERNAL_DXTN_LIB=1 \
		-DIN_DRI_DRIVER \
		-DGLX_DIRECT_RENDERING \
		-DGLX_INDIRECT_RENDERING \
		-DHZ=100 \
		-DHAVE_ALIAS \
		-DXF86VIDMODE \
		-UIN_DRI_DRIVER
.else
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden
CPPFLAGS+=	-DUSE_SSE_SYSCTL_DETECTION \
		-D__GLX_ALIGN64 \
		-DMESA_EXECMEM_MMAP \
		-DEXEC_HEAP_SIZE=10485760 \
		-D_NETBSD_SOURCE \
		-DUSE_NATIVE_LIBM_FUNCS \
		-DUSE_EXTERNAL_DXTN_LIB=1 \
		-DGLX_DIRECT_RENDERING \
		-DGLX_INDIRECT_RENDERING \
		-DHZ=100 \
		-DHAVE_ALIAS \
		-DXF86VIDMODE \
		-UIN_DRI_DRIVER
.endif # !defined(__MINIX)

LIBDPLIBS=	Xext	${.CURDIR}/../libXext \
		X11	${.CURDIR}/../libX11/dynamic \
		Xxf86vm	${.CURDIR}/../libXxf86vm \
		Xfixes	${.CURDIR}/../libXfixes \
		Xdamage	${.CURDIR}/../libXdamage \
		drm	${.CURDIR}/../libdrm \
		m	${.CURDIR}/../../../../../lib/libm

# XXX XXX
COPTS.dri_glx.c=			-Wno-error
COPTS.dri2_glx.c=			-Wno-error
COPTS.drisw_glx.c=			-Wno-error
COPTS.glxext.c=				-Wno-error
COPTS.indirect_vertex_program.c=	-Wno-error
MKLINT=no

PKGCONFIG=	gl
PKGDIST.gl=	${X11SRCDIR.MesaLib}/src/mesa
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.gl=	${MESA_VER}

CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-format -Wno-constant-conversion

.include <bsd.x11.mk>
.include <bsd.lib.mk>
.include <bsd.subdir.mk>
