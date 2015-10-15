#	$NetBSD: Makefile,v 1.8 2015/07/23 01:06:33 mrg Exp $

.include <bsd.own.mk>

LIB=	xcb
.PATH:	${X11SRCDIR.${LIB}}/src
.PATH:	${X11SRCDIR.${LIB}}/man
.PATH:	${X11SRCDIR.${LIB}}
.PATH:	${.CURDIR}/../files

SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	xcb_auth.c \
	xcb_conn.c \
	xcb_ext.c \
	xcb_in.c \
	xcb_list.c \
	xcb_out.c \
	xcb_util.c \
	xcb_xid.c

SRCS+=	xproto.c \
	bigreq.c \
	xc_misc.c

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-I${X11SRCDIR.${LIB}}/../include
#CPPFLAGS+=	-I${.CURDIR}/../files
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/xcb

LIBDPLIBS=\
	Xau	${.CURDIR}/../../libXau \
	Xdmcp	${.CURDIR}/../../libXdmcp

NOMAN=	# defined

PKGDIST= xcb

INCSDIR=${X11INCDIR}/xcb
INCS=	\
	bigreq.h \
	composite.h \
	damage.h \
	dpms.h \
	dri2.h \
	glx.h \
	randr.h \
	record.h \
	render.h \
	res.h \
	screensaver.h \
	shape.h \
	shm.h \
	sync.h \
	xc_misc.h \
	xcb.h \
	xcbext.h \
	xevie.h \
	xf86dri.h \
	xfixes.h \
	xinerama.h \
	xinput.h \
	xkb.h \
	xproto.h \
	xselinux.h \
	xtest.h \
	xv.h \
	xvmc.h

.include <bsd.x11.mk>
.include <bsd.lib.mk>
