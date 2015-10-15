#	$NetBSD: Makefile,v 1.16 2015/07/20 04:10:40 mrg Exp $

.include <bsd.own.mk>

LIB=	freetype

FREETYPE=	${X11SRCDIR.${LIB}}

SUBDIR=	freetype

.PATH:	${FREETYPE}/src/base
SRCS=	ftapi.c ftbase.c ftbbox.c ftbdf.c ftdebug.c ftfntfmt.c ftfstype.c \
	ftglyph.c ftinit.c ftlcdfil.c ftmm.c ftpfr.c ftstroke.c ftsynth.c \
	ftsystem.c fttype1.c ftwinfnt.c ftbitmap.c ftcid.c \
	ftgasp.c ftgxval.c ftotval.c ftpatent.c ftpfr.c

.PATH:	${FREETYPE}/src/autofit
SRCS+=	autofit.c

#.PATH:	${FREETYPE}/src/autohint
#SRCS+=	autohint.c

.PATH:	${FREETYPE}/src/bdf
SRCS+=	bdf.c

.PATH:	${FREETYPE}/src/cache
SRCS+=	ftcache.c

.PATH:	${FREETYPE}/src/cff
SRCS+=	cff.c

.PATH:	${FREETYPE}/src/cid
SRCS+=	type1cid.c

.PATH:	${FREETYPE}/src/gzip
SRCS+=	ftgzip.c

.PATH:	${FREETYPE}/src/bzip2
SRCS+=	ftbzip2.c

.PATH:	${FREETYPE}/src/lzw
SRCS+=	ftlzw.c

.PATH:	${FREETYPE}/src/pcf
SRCS+=	pcf.c

.PATH:	${FREETYPE}/src/pfr
SRCS+=	pfr.c

.PATH:	${FREETYPE}/src/psaux
SRCS+=	psaux.c

.PATH:	${FREETYPE}/src/pshinter
SRCS+=	pshinter.c

.PATH:	${FREETYPE}/src/psnames
SRCS+=	psmodule.c

.PATH:	${FREETYPE}/src/raster
SRCS+=	raster.c

.PATH:	${FREETYPE}/src/sfnt
SRCS+=	sfnt.c

.PATH:	${FREETYPE}/src/smooth
SRCS+=	smooth.c

.PATH:	${FREETYPE}/src/truetype
SRCS+=	truetype.c

.PATH:	${FREETYPE}/src/type1
SRCS+=	type1.c

.PATH:	${FREETYPE}/src/type42
SRCS+=	type42.c

.PATH:	${FREETYPE}/src/winfonts
SRCS+=	winfnt.c

CPPFLAGS+=	${X11FLAGS.THREADLIB}
CPPFLAGS+=	-DFT_CONFIG_OPTION_SYSTEM_ZLIB \
		-DFT_CONFIG_CONFIG_H="<ftconfig.h>" \
		-DFT2_BUILD_LIBRARY \
		-DFT_CONFIG_MODULES_H="<ftmodule.h>"
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/freetype2 \
		-I${DESTDIR}${X11INCDIR}/freetype2/config \
		-I${X11SRCDIR.${LIB}}/include

.if !defined(MKSUBPIXEL) || ${MKSUBPIXEL} != "no"
CPPFLAGS+=	-DFT_CONFIG_OPTION_SUBPIXEL_RENDERING
.endif

LIBDPLIBS= \
	z ${.CURDIR}/../../../../../lib/libz \
	bz2 ${.CURDIR}/../../../../../lib/libbz2

PKGCONFIG=	freetype2
PKGDIST.freetype2=	${.CURDIR}
PKGCONFIG_VERSION.freetype2=	${SHLIB_MAJOR}.${SHLIB_MINOR}.${SHLIB_TEENY}

.include <bsd.x11.mk>
.include <bsd.lib.mk>
.include <bsd.subdir.mk>
