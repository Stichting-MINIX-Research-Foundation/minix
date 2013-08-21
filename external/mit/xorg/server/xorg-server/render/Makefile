#	$NetBSD: Makefile,v 1.4 2011/08/02 09:11:39 mrg Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=		render

.PATH:		${X11SRCDIR.xorg-server}/render
SRCS=		animcur.c filter.c glyph.c matrix.c miindex.c mipict.c \
		mirect.c mitrap.c mitri.c picture.c render.c

CPPFLAGS+=      -I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/../include
CPPFLAGS+=      ${X11FLAGS.DIX}

COPTS.miglyph.c=-Wno-error	# XXX gcc 3.3
COPTS.mirect.c=	-Wno-error	# XXX gcc 3.3
COPTS.picture.c=	-Wno-error
COPTS.render.c=		-Wno-error

.include <bsd.x11.mk>
.include <bsd.lib.mk>
