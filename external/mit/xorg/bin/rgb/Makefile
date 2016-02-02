#	$NetBSD: Makefile,v 1.5 2015/07/22 07:58:00 mrg Exp $

.include <bsd.own.mk>

PROG=	showrgb

CPPFLAGS+=-DNDBM -DRGB_DB=\"${X11LIBDIR}/rgb\" -DUSE_RGB_TXT
CPPFLAGS+=-DHAVE_ASPRINTF
CPPFLAGS+=-I${X11SRCDIR.rgb}/include

X11EXTRAMANDEFS+=	-e 's,__RGB_DB__,${X11LIBDIR}/rgb,g' \
			-e 's,__RGB_DB_TYPE__,text,g'

FILES=	rgb.txt
FILESDIR=${X11LIBDIR}

.PATH:	${X11SRCDIR.rgb} ${X11SRCDIR.rgb}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
