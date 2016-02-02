# $NetBSD: dri.mk,v 1.1 2015/01/05 03:45:31 mrg Exp $

# XXX DRI_LIB_DEPS

LIBISMODULE=	yes

.include <bsd.own.mk>

SHLIB_MAJOR=	0

CPPFLAGS+= \
	-I${X11SRCDIR.MesaLib7}/src/mesa/drivers/dri/common \
	-I${X11SRCDIR.MesaLib7}/src/mesa/drivers/dri/${MODULE}/server \
	-I${X11SRCDIR.MesaLib7}/include \
	-I${X11SRCDIR.MesaLib7}/src/mapi \
	-I${X11SRCDIR.MesaLib7}/src/mesa \
	-I${DESTDIR}${X11INCDIR}/libdrm

.if !defined(__MINIX)
CPPFLAGS+=	-D_NETBSD_SOURCE -DPTHREADS -DUSE_EXTERNAL_DXTN_LIB=1 \
		-DIN_DRI_DRIVER -DGLX_DIRECT_RENDERING \
		-DGLX_INDIRECT_RENDERING -DHAVE_ALIAS -DHAVE_POSIX_MEMALIGN
.else
CPPFLAGS+=	-D_NETBSD_SOURCE -DUSE_EXTERNAL_DXTN_LIB=1 \
		-DIN_DRI_DRIVER -DGLX_DIRECT_RENDERING \
		-DGLX_INDIRECT_RENDERING -DHAVE_ALIAS -DHAVE_POSIX_MEMALIGN
.endif # !defined(__MINIX)

CPPFLAGS+=	-Wno-stack-protector

.PATH: ${X11SRCDIR.MesaLib7}/src/mesa/drivers/dri/${MODULE} ${DRI_EXTRA_PATHS}

# Common sources
.PATH:	${X11SRCDIR.MesaLib7}/src/mesa/drivers/dri/common \
	${X11SRCDIR.MesaLib7}/src/mesa/drivers/common
.if (${MODULE} == "swrast")
SRCS+=	drisw_util.c driverfuncs.c utils.c
.else
SRCS+=	dri_util.c drirenderbuffer.c driverfuncs.c texmem.c
SRCS+=	utils.c vblank.c xmlconfig.c
.endif

.include <bsd.x11.mk>

LIB=		${MODULE}_dri
LIBDIR=		${X11USRLIBDIR}/modules/dri

LIBDPLIBS+= 	drm		${.CURDIR}/../../libdrm
LIBDPLIBS+=	expat		${NETBSDSRCDIR}/external/mit/expat/lib/libexpat
LIBDPLIBS+=	m		${NETBSDSRCDIR}/lib/libm
LIBDPLIBS+= 	mesa_dri	${.CURDIR}/../libmesa
# to find mesa_dri.so
LDFLAGS+=	-Wl,-rpath,${LIBDIR}

.include <bsd.lib.mk>
