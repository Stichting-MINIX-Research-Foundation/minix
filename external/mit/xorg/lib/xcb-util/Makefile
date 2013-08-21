#	$NetBSD: Makefile,v 1.3 2012/01/07 02:07:58 mrg Exp $

SUBDIR= reply keysyms render-util \
	atom event aux .WAIT property .WAIT \
	image icccm

.include <bsd.subdir.mk>

INCSDIR=${X11INCDIR}/xcb
INCS=	xcb_atom.h xcb_aux.h xcb_bitops.h xcb_event.h xcb_icccm.h \
	xcb_image.h xcb_keysyms.h xcb_pixel.h xcb_property.h \
	xcb_renderutil.h xcb_reply.h

.include <bsd.inc.mk>

.PATH:	${X11SRCDIR.xcb-util}/atom
.PATH:	${X11SRCDIR.xcb-util}/aux
.PATH:	${X11SRCDIR.xcb-util}/event
.PATH:	${X11SRCDIR.xcb-util}/icccm
.PATH:	${X11SRCDIR.xcb-util}/image
.PATH:	${X11SRCDIR.xcb-util}/keysyms
.PATH:	${X11SRCDIR.xcb-util}/property
.PATH:	${X11SRCDIR.xcb-util}/renderutil
.PATH:	${X11SRCDIR.xcb-util}/reply
