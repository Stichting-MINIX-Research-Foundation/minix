#	$NetBSD: Makefile,v 1.10 2015/07/20 04:10:40 mrg Exp $

NOMAN=		1

.include <bsd.own.mk>

HOSTPROG=	fc-cache

FONTCONFIG=	${X11SRCDIR.fontconfig}
FREETYPE=	${X11SRCDIR.freetype}
EXPAT=		${NETBSDSRCDIR}/external/mit/expat/dist

.PATH:	${FONTCONFIG}/fc-cache
SRCS.fc-cache=	fc-cache.c

.PATH:	${FONTCONFIG}/src
SRCS.fontconfig=	fcatomic.c fcblanks.c fccache.c fccfg.c \
		fccharset.c fcdbg.c fccompat.c fcdefault.c fcdir.c \
		fcfreetype.c fcfs.c fchash.c fcinit.c fclang.c \
		fclist.c fcmatch.c fcmatrix.c fcname.c fcobjs.c \
		fcpat.c fcserialize.c fcstat.c fcstr.c fcxml.c \
		ftglue.c

.PATH:	${FREETYPE}/src/base
SRCS.freetype=	ftapi.c ftbase.c ftbbox.c ftbdf.c ftdebug.c ftfntfmt.c \
		ftglyph.c ftinit.c ftmm.c ftpfr.c ftstroke.c ftsynth.c \
		ftsystem.c fttype1.c ftwinfnt.c ftbitmap.c

.PATH: ${FREETYPE}/src/autofit
SRCS.freetype+=	autofit.c

.PATH:	${FREETYPE}/src/bdf
SRCS.freetype+=	bdf.c

.PATH:	${FREETYPE}/src/cff
SRCS.freetype+=	cff.c

.PATH:	${FREETYPE}/src/cid
SRCS.freetype+=	type1cid.c

.PATH:	${FREETYPE}/src/gzip
SRCS.freetype+=	ftgzip.c

#.PATH:	${FREETYPE}/src/bzip2
#SRCS.freetype+=	ftbzip2.c

.PATH:	${FREETYPE}/src/lzw
SRCS.freetype+=	ftlzw.c

.PATH:	${FREETYPE}/src/pcf
SRCS.freetype+=	pcf.c

.PATH:	${FREETYPE}/src/pfr
SRCS.freetype+=	pfr.c

.PATH:	${FREETYPE}/src/psaux
SRCS.freetype+=	psaux.c

.PATH:	${FREETYPE}/src/pshinter
SRCS.freetype+=	pshinter.c

.PATH:	${FREETYPE}/src/psnames
SRCS.freetype+=	psnames.c

.PATH:	${FREETYPE}/src/raster
SRCS.freetype+=	raster.c

.PATH:	${FREETYPE}/src/sfnt
SRCS.freetype+=	sfnt.c

.PATH:	${FREETYPE}/src/smooth
SRCS.freetype+=	smooth.c

.PATH:	${FREETYPE}/src/truetype
SRCS.freetype+=	truetype.c

.PATH:	${FREETYPE}/src/type1
SRCS.freetype+=	type1.c

.PATH:	${FREETYPE}/src/type42
SRCS.freetype+=	type42.c

.PATH:	${FREETYPE}/src/winfonts
SRCS.freetype+=	winfnt.c

.PATH:	${EXPAT}/lib
SRCS.expat=	xmlparse.c xmltok.c xmlrole.c

SRCS=	${SRCS.fc-cache} ${SRCS.fontconfig} ${SRCS.freetype} ${SRCS.expat}

# XXX FONTCONFIG_PATH was arbitrarily chosen
HOST_CPPFLAGS=	-DFONTCONFIG_PATH='"${DESTDIR}/etc/X11/fonts"' \
		-DFC_DEFAULT_FONTS='"${X11FONTDIR}/Type1"' \
		-DFC_CACHEDIR='"${DESTDIR}/var/cache/fontconfig"' \
		-DHAVE_FT_BITMAP_SIZE_Y_PPEM \
		-DHAVE_FT_GET_BDF_PROPERTY \
		-DHAVE_FT_GET_NEXT_CHAR \
		-DHAVE_FT_GET_PS_FONT_INFO \
		-DHAVE_FT_GET_X11_FONT_FORMAT \
		-DHAVE_FT_HAS_PS_GLYPH_NAMES \
		-DHAVE_EXPAT -DXFREE86_FT2 -DHAVE_INTTYPES_H \
		-DFT2_BUILD_LIBRARY \
		-DXML_BYTE_ORDER=0 -DHAVE_MEMMOVE=1 \
		-DHAVE_STDINT_H -DHAVE_RANDOM -DDARWIN_NO_CARBON \
		-DHAVE_SYS_TYPES_H -DHAVE_FCNTL_H -DHAVE_SYS_STAT_H \
		-DHAVE_MKSTEMP \
		-DHAVE_SCANDIR \
		-DFLEXIBLE_ARRAY_MEMBER="/**/"
HOST_CPPFLAGS+=	-DFT_CONFIG_OPTION_DISABLE_BZIP2
HOST_CPPFLAGS+=	-I${FONTCONFIG} -I${FREETYPE} \
		-I${FREETYPE}/include -I${EXPAT}/lib \
		-I${FONTCONFIG}/../include \
		-I${DESTDIR}${X11INCDIR} -I.

LDADD=	-lz
#LDADD+=	-lbz2

.include "../../lib/fontconfig/src/Makefile.fcarch"

.include <bsd.x11.mk>
.include <bsd.hostprog.mk>
