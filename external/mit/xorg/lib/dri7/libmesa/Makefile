#	$NetBSD: Makefile,v 1.1 2015/01/05 03:45:31 mrg Exp $

.include <bsd.own.mk>

_LIB_PREFIX=	lib
LIB=		mesa_dri
LIBISMODULE=	yes
LIBISCXX=	yes

SHLIB_MAJOR=	0

MESA_SRC_MODULES=	main math vbo tnl program swrast ss common

CPPFLAGS+=	\
	-I${X11SRCDIR.MesaLib7}/include \
	-I${X11SRCDIR.MesaLib7}/src/glsl \
	-I${X11SRCDIR.MesaLib7}/src/mesa \
	-I${X11SRCDIR.MesaLib7}/src/mapi

.if !defined(__MINIX)
CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DPTHREADS -D_NETBSD_SOURCE -DUSE_EXTERNAL_DXTN_LIB=1 \
		-DHAVE_ALIAS -DIN_DRI_DRIVER
#CPPFLAGS+=	-DGLX_DIRECT_RENDERING -DGLX_INDIRECT_RENDERING -DHAVE_POSIX_MEMALIGN
CFLAGS+=	-pthread
.else
CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-D_NETBSD_SOURCE -DUSE_EXTERNAL_DXTN_LIB=1 \
		-DHAVE_ALIAS -DIN_DRI_DRIVER
.endif # !defined(__MINIX)

.include "../libmesa7.mk"
.include "../libglsl7.mk"

SRCS+=  ${SRCS.glsl}

.include <bsd.x11.mk>

LIBDIR=		${X11USRLIBDIR}/modules/dri

#LIBDPLIBS+=	GL	${.CURDIR}/../../libGL

CWARNFLAGS.clang+=	-Wno-format -Wno-constant-conversion \
			-Wno-tautological-compare

.include <bsd.lib.mk>
