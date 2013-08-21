#	$NetBSD: Makefile,v 1.3 2010/11/21 02:33:40 mrg Exp $

.include <bsd.own.mk>

PROG=	showrgb

CPPFLAGS+=-DNDBM -DRGB_DB=\"${X11LIBDIR}/rgb\" -DUSE_RGB_TXT
CPPFLAGS+=-I${X11SRCDIR.rgb}/include

X11EXTRAMANDEFS+=	-e 's,__RGB_DB__,${X11LIBDIR}/rgb,g; \
			    s,__RGB_DB_TYPE__,text,g'

FILES=	rgb.txt
FILESDIR=${X11LIBDIR}

.PATH:	${X11SRCDIR.rgb} ${X11SRCDIR.rgb}/man

.include <bsd.x11.mk>
.include <bsd.prog.mk>
