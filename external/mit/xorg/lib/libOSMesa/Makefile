#	$NetBSD: Makefile,v 1.7 2012/03/22 23:46:27 joerg Exp $

.include <bsd.own.mk>

LIB=		OSMesa

MESA_SRC_MODULES=	main math vbo tnl shader swrast slang glapi common \
			osmesa ss

.PATH:		${X11SRCDIR.MesaLib}/include/GL
INCS=		osmesa.h
INCSDIR=	${X11INCDIR}/GL

.if !defined(__MINIX)
CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DPTHREADS -DCHAN_BITS=32 -DDEFAULT_SOFTWARE_DEPTH_BITS=31 \
		-D_NETBSD_SOURCE -DUSE_EXTERNAL_DXTN_LIB=1 -DHAVE_ALIAS
CFLAGS+=	-pthread
.else
CPPFLAGS+=	${X11FLAGS.THREADLIB} \
		-DCHAN_BITS=32 -DDEFAULT_SOFTWARE_DEPTH_BITS=31 \
		-D_NETBSD_SOURCE -DUSE_EXTERNAL_DXTN_LIB=1 -DHAVE_ALIAS
.endif # !defined(__MINIX)

CWARNFLAGS.clang+=	-Wno-format -Wno-constant-conversion

.include "libmesa.mk"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
