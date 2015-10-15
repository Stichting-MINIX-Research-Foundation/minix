#	$NetBSD: Makefile,v 1.21 2014/12/18 06:24:30 mrg Exp $

.include <bsd.own.mk>

LIB=		GL
LIBISCXX=	yes

SUBDIR=		internal

# glx
.PATH:		${X11SRCDIR.MesaLib}/src/glx
SRCS.glx=	\
	clientattrib.c \
	clientinfo.c \
	compsize.c \
	create_context.c \
	eval.c \
	glxconfig.c \
	glxcmds.c \
	glxcurrent.c \
	glx_error.c \
	glxext.c \
	glxextensions.c \
	indirect_glx.c \
	MESAindirect.c \
	MESAindirect_init.c \
	MESAindirect_size.c \
	MESAindirect_window_pos.c \
	indirect_texture_compression.c \
	indirect_transpose_matrix.c \
	indirect_vertex_array.c \
	indirect_vertex_program.c \
	pixel.c \
	pixelstore.c \
	query_renderer.c \
	render2.c \
	renderpix.c \
	single2.c \
	singlepix.c \
	vertarr.c \
	glx_pbuffer.c \
	glx_query.c \
	glxhash.c

BUILDSYMLINKS+=     ${X11SRCDIR.MesaLib}/../src/mapi/glapi/indirect.c MESAindirect.c
BUILDSYMLINKS+=     ${X11SRCDIR.MesaLib}/../src/mapi/glapi/indirect_init.c MESAindirect_init.c
BUILDSYMLINKS+=     ${X11SRCDIR.MesaLib}/../src/mapi/glapi/indirect_size.c MESAindirect_size.c
BUILDSYMLINKS+=     ${X11SRCDIR.MesaLib}/src/glx/indirect_window_pos.c MESAindirect_window_pos.c

.for _f in ${SRCS.glx}
CPPFLAGS.${_f}= -I${X11SRCDIR.MesaLib}/../src/mapi/glapi -DGLX_SHARED_GLAPI
.endfor

# glapi
.PATH:		${X11SRCDIR.MesaLib}/src/mapi
.PATH:		${X11SRCDIR.MesaLib}/../src/mapi/glapi
SRCS.glapi=	\
	entry.c \
	glapi_gentable.c
.for _f in ${SRCS.glapi}
CPPFLAGS.${_f}= -DMAPI_MODE_BRIDGE \
		-DMAPI_ABI_HEADER=\"glapi_glapi_mapi_tmp.h\" \
		-I${X11SRCDIR.MesaLib}/src/mesa
.endfor


# XXX see TODO
#.if ${MACHINE_ARCH} == "i386"
#.PATH:		${X11SRCDIR.MesaLib}/src/mesa/x86
#SRCS.glx+=	glapi_x86.S
#CPPFLAGS+=	-DUSE_X86_ASM -I${X11SRCDIR.MesaLib}/src/mesa/x86
## -DUSE_X86_ASM $(MMX_DEFS) $(3DNOW_DEFS) $(SSE_DEFS)
#.endif


.if ${X11DRI} != "no"
# dri
SRCS.dri=	\
	  drisw_glx.c \
          xfont.c \
          dri_common.c \
          dri_glx.c \
          XF86dri.c \
          dri2_glx.c \
          dri2.c \
          dri2_query_renderer.c
.for fn in ${SRCS.dri}
CPPFLAGS.${fn}=-DDEFAULT_DRIVER_DIR=\"${X11USRLIBDIR}/modules/dri\"
.endfor
.endif

.include "../libloader.mk"

.PATH:		${X11SRCDIR.MesaLib}/src/glw
.PATH:		${X11SRCDIR.MesaLib}/include/GL
.PATH:		${X11SRCDIR.MesaLib}/src/mesa/drivers/x11
INCS=	gl.h gl_mangle.h glcorearb.h glext.h \
	glx.h glx_mangle.h glxext.h \
	osmesa.h wglext.h wmesa.h
INCSDIR=${X11INCDIR}/GL


SRCS+=	${SRCS.dri} ${SRCS.glx} ${SRCS.glapi}

CPPFLAGS+= \
	-I${X11SRCDIR.MesaLib}/include \
	-I${X11SRCDIR.MesaLib}/include/GL/internal \
	-I${X11SRCDIR.MesaLib}/src/loader \
	-I${X11SRCDIR.MesaLib}/src/mapi \
	-I${X11SRCDIR.MesaLib}/src/mapi/glapi \
	-I${X11SRCDIR.MesaLib}/../src/mapi/glapi \
	-I${X11SRCDIR.MesaLib}/src/glx \
	-I${DESTDIR}${X11INCDIR}/libdrm \
	-I${DESTDIR}${X11INCDIR}/GL \
	-I${DESTDIR}${X11INCDIR}/X11 \
	-I${DESTDIR}${X11INCDIR}/X11/extensions

CPPFLAGS+=	${X11FLAGS.THREADLIB}

#		-D_NETBSD_SOURCE \

# old
CPPFLAGS+=	-D__GLX_ALIGN64 \
		-DXF86VIDMODE \
		-DGLX_SHARED_GLAPI

# new
CPPFLAGS+=	\
	-DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 \
	-DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 \
	-DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 \
	-DHAVE_UNISTD_H=1 -DHAVE_DLFCN_H=1 -DHAVE___BUILTIN_BSWAP32=1 \
	-DHAVE___BUILTIN_BSWAP64=1 -DHAVE_DLADDR=1 -DHAVE_CLOCK_GETTIME=1 \
	-DHAVE_POSIX_MEMALIGN -DHAVE_PTHREAD=1 -DHAVE_DLOPEN

.include "../asm.mk"

CPPFLAGS+=	\
	-DHAVE_LIBDRM -DGLX_USE_DRM \
	-DGLX_INDIRECT_RENDERING -DGLX_DIRECT_RENDERING \
	-DHAVE_ALIAS -DMESA_EGL_NO_X11_HEADERS

CPPFLAGS+=	\
	-DUSE_EXTERNAL_DXTN_LIB=1 \
	-DYYTEXT_POINTER=1

CFLAGS+=	-fvisibility=hidden -fno-strict-aliasing -fno-builtin-memcmp
.if !defined(__MINIX)
CFLAGS+=	-pthread
.else
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
CWARNFLAGS.clang+=	-Wno-\#warnings
.endif # !defined(__MINIX)

LIBDPLIBS=	Xext		${.CURDIR}/../libXext \
		X11		${.CURDIR}/../libX11/dynamic \
		Xxf86vm		${.CURDIR}/../libXxf86vm \
		Xfixes		${.CURDIR}/../libXfixes \
		Xdamage		${.CURDIR}/../libXdamage \
		glapi		${.CURDIR}/../libglapi \
		drm		${.CURDIR}/../libdrm \
		X11-xcb		${.CURDIR}/../libX11/libX11-xcb \
		xcb-dri2	${.CURDIR}/../libxcb/dri2 \
		xcb-glx		${.CURDIR}/../libxcb/glx \
		expat		${NETBSDSRCDIR}/external/mit/expat/lib/libexpat \
		m		${NETBSDSRCDIR}/lib/libm

MKLINT=no

PKGCONFIG=	gl
PKGDIST.gl=	${X11SRCDIR.MesaLib}/src/mesa
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.gl=	${MESA_VER}

# XXX remove these from bsd.x11.mk
# XXX enable GLX TLS?  it's enabled in the code, but not here..
PKGCONFIG_SED_FLAGS= \
	-e "s,@GL_PC_REQ_PRIV@,x11 xext,; \
	    s,@GL_LIB@,GL,; \
	    s,@GL_PC_LIB_PRIV@,-lm -lpthread,; \
	    s,@GL_PC_CFLAGS@,,; \
	    s,@GLX_TLS@,no,"


CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-format -Wno-constant-conversion

.include <bsd.x11.mk>
.include <bsd.lib.mk>
.include <bsd.subdir.mk>
