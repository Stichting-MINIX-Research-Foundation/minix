#	$NetBSD: Makefile,v 1.1 2014/12/18 06:24:30 mrg Exp $

.include <bsd.own.mk>

LIB=		glapi
LIBISCXX=	yes

SHLIB_MAJOR=    0
SHLIB_MINOR=    0

# util
.PATH:		${X11SRCDIR.MesaLib}/src/util
SRCS.util=	\
	hash_table.c    \
        ralloc.c
.PATH:		${X11SRCDIR.MesaLib}/../src/util
SRCS.util+=	\
	format_srgb.c

# mapi
.PATH:		${X11SRCDIR.MesaLib}/src/mapi
SRCS.mapi=	\
	entry.c \
	mapi_glapi.c \
	stub.c \
	table.c \
	u_current.c \
	u_execmem.c
CPPFLAGS.entry.c=	-DMAPI_MODE_GLAPI -DMAPI_ABI_HEADER=\"shared-glapi_glapi_mapi_tmp.h\"

BUILDSYMLINKS+=   ${X11SRCDIR.MesaLib}/src/mapi/entry.c shared_entry.c

# above is shared/libglapi.la

SRCS+=	${SRCS.util} ${SRCS.mapi} ${SRCS.mapi-glapi}

CPPFLAGS+= \
	-I${X11SRCDIR.MesaLib}/include \
	-I${X11SRCDIR.MesaLib}/src/mapi \
	-I${X11SRCDIR.MesaLib}/src/mapi/glapi \
	-I${X11SRCDIR.MesaLib}/src/mesa \
	-I${X11SRCDIR.MesaLib}/src/util \
	-I${X11SRCDIR.MesaLib}/../src/mapi \
	-I${X11SRCDIR.MesaLib}/../src/mapi/glapi \
	-I${X11SRCDIR.MesaLib}/../src/mesa

CPPFLAGS+= \
	-DMAPI_MODE_GLAPI -DMAPI_ABI_HEADER=\"shared-glapi_glapi_mapi_tmp.h\"

CPPFLAGS+=	${X11FLAGS.THREADLIB}

.include "${.CURDIR}/../libGL/mesa-ver.mk"

.if !defined(__MINIX)
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden -pthread -fno-builtin-memcmp
.else
CFLAGS+=	-fno-strict-aliasing -fvisibility=hidden -fno-builtin-memcmp
LDADD+= -lmthread
DPADD+= ${LIBMTHREAD}
.endif # !defined(__MINIX)

CPPFLAGS+=	\
	-DPACKAGE_NAME=\"Mesa\" \
	-DPACKAGE_TARNAME=\"mesa\" \
	-DPACKAGE_BUGREPORT=\"https://bugs.freedesktop.org/enter_bug.cgi?product=Mesa\" \
	-DPACKAGE_VERSION=\"${MESA_VER}\" \
	-DPACKAGE_STRING=\"Mesa\ ${MESA_VER}\" \
	-DVERSION=\"${MESA_VER}\" \
	-DPACKAGE_URL=\"\" \
	-DPACKAGE=\"mesa\" \
	-DSTDC_HEADERS=1 \
	-DHAVE_SYS_TYPES_H=1 \
	-DHAVE_SYS_STAT_H=1 \
	-DHAVE_STDLIB_H=1 \
	-DHAVE_STRING_H=1 \
	-DHAVE_MEMORY_H=1 \
	-DHAVE_STRINGS_H=1 \
	-DHAVE_INTTYPES_H=1 \
	-DHAVE_STDINT_H=1 \
	-DHAVE_UNISTD_H=1 \
	-DHAVE_DLFCN_H=1 \
	-DYYTEXT_POINTER=1 \
	-DHAVE___BUILTIN_BSWAP32=1 \
	-DHAVE___BUILTIN_BSWAP64=1 \
	-DHAVE_DLADDR=1 \
	-DHAVE_CLOCK_GETTIME=1 \
	-DHAVE_PTHREAD=1 \
	-I. \
	-DUSE_EXTERNAL_DXTN_LIB=1 \
	-DHAVE_DLOPEN \
	-DHAVE_POSIX_MEMALIGN \
	-DHAVE_LIBDRM \
	-DGLX_USE_DRM \
	-DGLX_INDIRECT_RENDERING \
	-DGLX_DIRECT_RENDERING \
	-DHAVE_PTHREAD \
	-DHAVE_ALIAS \
	-DMESA_EGL_NO_X11_HEADERS

.include "../asm.mk"

MKLINT=no

#CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-format -Wno-constant-conversion

.include <bsd.x11.mk>
.include <bsd.lib.mk>
