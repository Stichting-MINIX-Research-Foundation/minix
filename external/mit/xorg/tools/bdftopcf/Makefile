#	$NetBSD: Makefile,v 1.6 2013/05/31 05:24:50 mrg Exp $

NOMAN=		1

.include <bsd.own.mk>

HOSTPROG=	bdftopcf
.PATH:		${X11SRCDIR.bdftopcf}

FREETYPE=       ${X11SRCDIR.freetype}

SRCS=		bdftopcf.c

HOST_CPPFLAGS+=	-I${DESTDIR}${X11INCDIR} \
		-I${DESTDIR}${X11INCDIR} \
		-I${DESTDIR}${X11INCDIR}/X11/fonts \
		-I${DESTDIR}${X11INCDIR}/freetype2 \
		-I${X11SRCDIR.Xfont}/src/stubs \
		-DBDFFORMAT -DPCFFORMAT -DSNFFORMAT -DX_GZIP_FONT_COMPRESSION \
		-DFONT_ENCODINGS_DIRECTORY=\"${X11FONTDIR}/encodings/encodings.dir\" \
		-DPACKAGE_STRING="\"NetBSD tool bdftopcf 1.0.4\""

.PATH:		${X11SRCDIR.Xfont}/src/bitmap
SRCS+=		bdfread.c bdfutils.c bitmap.c bitmaputil.c fontink.c \
		pcfread.c pcfwrite.c

.PATH:		${X11SRCDIR.Xfont}/src/fontfile
SRCS+=		bufio.c decompress.c defaults.c fileio.c filewr.c gunzip.c

.PATH:		${X11SRCDIR.Xfont}/src/util
SRCS+=		atom.c fontaccel.c private.c utilbitmap.c

LDADD+=		-lz

.include <bsd.x11.mk>
.include <bsd.hostprog.mk>
