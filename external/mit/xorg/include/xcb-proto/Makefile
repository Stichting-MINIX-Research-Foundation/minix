#	$NetBSD: Makefile,v 1.2 2013/05/31 09:14:19 mrg Exp $

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.xcb-proto} ${X11SRCDIR.xcb-proto}/src

FILES=	bigreq.xml composite.xml damage.xml dpms.xml dri2.xml \
	ge.xml glx.xml randr.xml record.xml render.xml res.xml \
	screensaver.xml shape.xml shm.xml sync.xml xc_misc.xml \
	xcb.xsd xevie.xml xf86dri.xml xf86vidmode.xml xfixes.xml \
	xinerama.xml xkb.xml \
	xinput.xml xprint.xml xproto.xml xselinux.xml xtest.xml \
	xv.xml xvmc.xml
FILESDIR=${X11SHAREDIR}/xcb

PKGCONFIG=	xcb-proto

NOPROG=	# defined

.include <bsd.x11.mk>
.include <bsd.files.mk>
.include <bsd.prog.mk>
