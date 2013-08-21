#	$NetBSD: Makefile,v 1.8 2011/02/21 04:42:17 mrg Exp $

.include <bsd.sys.mk> # for HOST_SH

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	init

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/common
SRCS=	xf86Init.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support/bus \
                -I${X11SRCDIR.xorg-server}/mfb \
                -I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/os \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
                -I${X11SRCDIR.xorg-server}/Xi \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/ddc \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/i2c \
		-I${X11SRCDIR.xorg-server}/hw/xfree86 \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/parser \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/loader \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/vgahw \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/rac \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/scanpci \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/vbe \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/int10 \
		-I${X11SRCDIR.xorg-server}/render \
		-I${X11SRCDIR.xorg-server}/randr \
		-I${X11SRCDIR.xorg-server}/../include \
		-I.

.if !defined(__MINIX)
CPPFLAGS+=	${X11FLAGS.EXTENSION} ${X11FLAGS.OS_DEFINES} \
		${X11FLAGS.DIX} \
		-DXFree86LOADER -DWSCONS_SUPPORT -D__XKBDEFRULES__='"xorg"' \
		-D__XSERVERNAME__='"Xorg"' -D__XCONFIGFILE__='"xorg.conf"' \
		-D__VENDORDWEBSUPPORT__='"http://wiki.X.Org"' \
		-DXVENDORNAME='"The X.Org Foundation"' \
		-DHAVE_XORG_CONFIG_H
.else
CPPFLAGS+=	${X11FLAGS.EXTENSION} ${X11FLAGS.OS_DEFINES} \
		${X11FLAGS.DIX} \
		-DXFree86LOADER -D__XKBDEFRULES__='"xorg"' \
		-D__XSERVERNAME__='"Xorg"' -D__XCONFIGFILE__='"xorg.conf"' \
		-D__VENDORDWEBSUPPORT__='"http://wiki.X.Org"' \
		-DXVENDORNAME='"The X.Org Foundation"' \
		-DHAVE_XORG_CONFIG_H
.endif # !defined(__MINIX)

.if ${MACHINE_ARCH} == "i386"
CPPFLAGS+=	-DPCCONS_SUPPORT -DPCVT_SUPPORT
.endif

.if ${MACHINE} == "cats" || ${MACHINE} == "ofppc"
CPPFLAGS+=	-DPCVT_SUPPORT
.endif

CPPFLAGS.xf86Init.c=	-DOSNAME='"NetBSD/${MACHINE} ${OS_RELEASE} -"' \
			-DXORG_RELEASE='"Release 1.4.2"' \
			-DBUILDERADDR='"xorg@xorg.org"' \
			-DDEFAULT_MODULE_PATH=\"${X11USRLIBDIR}/modules\" \
			-DDEFAULT_LIBRARY_PATH=\"${X11USRLIBDIR}\"

.include <bsd.x11.mk>
.include <bsd.lib.mk>
