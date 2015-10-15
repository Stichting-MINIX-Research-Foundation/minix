#	$NetBSD: Makefile,v 1.1 2015/02/23 00:09:30 jmcneill Exp $

.include <bsd.own.mk>

LIB=		gbm

.PATH:	${X11SRCDIR.MesaLib}/src/gbm/main
SRCS.main=	\
	backend.c \
	gbm.c

.PATH:	${X11SRCDIR.MesaLib}/src/gbm/backends/dri
SRCS.dri=	\
	gbm_dri.c

SRCS=	${SRCS.main} ${SRCS.dri}

CPPFLAGS+=	-D_OS_UNIX=1
CPPFLAGS+=	-DHAVE_PTHREAD=1
CPPFLAGS+=	-DDEFAULT_DRIVER_DIR=\"${X11USRLIBDIR}/modules/dri\"
CPPFLAGS+=	-DMODULEDIR=\"${X11USRLIBDIR}/gbm\"
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/include
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/loader
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/gbm/main
CPPFLAGS+=	-I${X11SRCDIR.MesaLib}/src/mapi
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm
CFLAGS+=	-fno-strict-aliasing -fexceptions
.if defined(__MINIX)
CWARNFLAGS.clang+= -Wno-\#warnings
.endif # defined(__MINIX)

COPTS.gbm_dri.c+=	-Wno-error

.include "../libloader.mk"

.PATH:	${X11SRCDIR.MesaLib}/src/gbm/main
INCS=		gbm.h
INCSDIR=	${X11INCDIR}

LIBDPLIBS=      glapi	${.CURDIR}/../libglapi
LIBDPLIBS=      drm     ${.CURDIR}/../libdrm

PKGCONFIG=	gbm
PKGDIST.gbm=	${X11SRCDIR.MesaLib}/src/gbm/main
.include "${.CURDIR}/../libGL/mesa-ver.mk"
PKGCONFIG_VERSION.gbm=	${MESA_VER}

.include "../libdrm/Makefile.defines"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
