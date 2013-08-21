#	$NetBSD: Makefile,v 1.18 2011/04/13 16:18:14 macallan Exp $

.include <bsd.own.mk> # MINIX: for __MINIX
.include <bsd.sys.mk> # for HOST_SH

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	common

.PATH:	${X11SRCDIR.xorg-server}/hw/xfree86/common
SRCS.xv=	xf86xv.c xf86xvmc.c
SRCS.bus=	xf86pciBus.c xf86fbBus.c xf86noBus.c
SRCS.randr=	xf86RandR.c

SRCS=	xf86Configure.c xf86ShowOpts.c xf86Bus.c xf86Config.c \
	xf86Cursor.c xf86DGA.c xf86DPMS.c \
	xf86Events.c xf86Globals.c xf86AutoConfig.c \
	xf86Option.c \
	xf86VidMode.c xf86fbman.c xf86cmap.c \
	xf86Helper.c xf86PM.c xf86Xinput.c xisb.c \
	xf86Mode.c xorgHelper.c \
	${SRCS.xv} ${SRCS.bus} ${SRCS.randr} \
	xf86DefModeSet.c \
	xf86VGAarbiter.c

.if ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64"
SRCS.bus+=	xf86sbusBus.c
.endif

# Rebuild xf86Build.h on every make
#
CLEANFILES+=	xf86Build.h
xf86Build.h: .PHONY
	${_MKTARGET_CREATE}
	echo "#define BUILD_DATE `date +%Y%m%d`" > ${.TARGET}

# We don't use  DPSRCS+=xf86Build.h  because we don't want the entire
# library to be rebuilt each time
#
xf86Init.c xf86Init.o: xf86Build.h

CLEANFILES+=	xf86DefModeSet.c
xf86DefModeSet.c:
	${_MKTARGET_CREATE}
	cat \
	${X11SRCDIR.xorg-server}/hw/xfree86/common/vesamodes \
	${X11SRCDIR.xorg-server}/hw/xfree86/common/extramodes | \
	${TOOL_AWK} -f ${X11SRCDIR.xorg-server}/hw/xfree86/common/modeline2c.awk > ${.TARGET}
	@echo >> ${.TARGET}

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
		-I${X11SRCDIR.xorg-server}/hw/xfree86/modes \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/i2c \
		-I${X11SRCDIR.xorg-server}/hw/xfree86 \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/parser \
                -I${X11SRCDIR.xorg-server}/hw/xfree86/loader \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/vgahw \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/rac \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/vbe \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/int10 \
		-I${X11SRCDIR.xorg-server}/render \
		-I${X11SRCDIR.xorg-server}/randr \
		-I. \
		-I${X11SRCDIR.xorg-server}/../include \
		-D__${MACHINE}

.if !defined(__MINIX)
CPPFLAGS+=	${X11FLAGS.EXTENSION} ${X11FLAGS.OS_DEFINES} \
		${X11FLAGS.DIX} \
		-DXFree86LOADER -DWSCONS_SUPPORT -D__XKBDEFRULES__='"xorg"' \
		-D__XSERVERNAME__='"Xorg"' -D__XCONFIGFILE__='"xorg.conf"' \
		-D__VENDORDWEBSUPPORT__='"http://wiki.X.Org"' \
		-DXVENDORNAME='"The X.Org Foundation"' \
		-DHAVE_XORG_CONFIG_H \
		-DHAVE_PCI_DEVICE_IS_BOOT_VGA
.else
CPPFLAGS+=	${X11FLAGS.EXTENSION} ${X11FLAGS.OS_DEFINES} \
		${X11FLAGS.DIX} \
		-DXFree86LOADER -D__XKBDEFRULES__='"xorg"' \
		-D__XSERVERNAME__='"Xorg"' -D__XCONFIGFILE__='"xorg.conf"' \
		-D__VENDORDWEBSUPPORT__='"http://wiki.X.Org"' \
		-DXVENDORNAME='"The X.Org Foundation"' \
		-DHAVE_XORG_CONFIG_H \
		-DHAVE_PCI_DEVICE_IS_BOOT_VGA
.endif # !defined(__MINIX)

.if ${MACHINE_ARCH} == "i386"
.if !defined(__MINIX)
CPPFLAGS+=	-DPCCONS_SUPPORT -DPCVT_SUPPORT
.else
CPPFLAGS+=	-DPCCONS_SUPPORT
.endif # !defined(__MINIX)
.endif

.if ${MACHINE} == "cats" || ${MACHINE} == "ofppc"
CPPFLAGS+=	-DPCVT_SUPPORT
.endif

OS_RELEASE!=	${HOST_SH} ${NETBSDSRCDIR}/sys/conf/osrelease.sh

DRIVERS=	mga glint nv tga s3 s3virge sis rendition neomagic \
		i740 tdfx savage cirrus vmware tseng trident chips \
		apm i128
DRIVERS+=	ati i810 ark cyrix siliconmotion via fbdev vesa \
		dummy

IDRIVERS=	mouse keyboard acecad calcomp citron digitaledge \
		dmc dynapro elographics tek4957 microtouch \
		mutouch penmount spaceorb summa wacom void \
		magellan hyperpen jamstudio fpit palmax

CPPFLAGS.xf86Bus.c=		-DBUILDERADDR='"xfree86@xfree86.org"'
CPPFLAGS.xf86Init.c=		-DOSNAME='"NetBSD/${MACHINE} ${OS_RELEASE} -"' \
				-DXORG_RELEASE='"Release 1.4.2"' \
				-DBUILDERADDR='"xorg@xorg.org"' \
				-DOSVENDOR='" The NetBSD Foundation, Inc."'
CPPFLAGS.xf86Globals.c=		-DDEFAULT_LOGPREFIX='"/var/log/Xorg."' \
				-DDEFAULT_MODULE_PATH='"${X11USRLIBDIR}/modules"'
CPPFLAGS.xf86Configure.c=	-DXF86CONFIGFILE='"xorg.conf"'
CPPFLAGS.xf86Config.c=		-DDRIVERS='"${DRIVERS}"' \
				-DIDRIVERS='"${IDRIVERS}"' \
				-DPROJECTROOT='"${X11ROOTDIR}"'
CPPFLAGS.xf86AutoConfig.c=	-DPROJECTROOT='"${X11ROOTDIR}"' \
				-DBINDIR='"${X11BINDIR}"' \
				-DDRIVERS='"${DRIVERS}"' \
				-DIDRIVERS='"${IDRIVERS}"'

COPTS.xf86AutoConfig.c=		-Wno-error
COPTS.xf86Config.c=		-Wno-error
COPTS.xf86Cursor.c=		-Wno-error
COPTS.xf86RandR.c=		-Wno-error
COPTS.xf86Helper.c=		-Wno-error
COPTS.xf86cmap.c=		-Wno-error
COPTS.xf86Cursor.c=		-Wno-error	# XXX xf86dgastr.h
COPTS.xf86DGA.c=		-Wno-error	# XXX xf86dgastr.h

.include <bsd.x11.mk>
.include <bsd.lib.mk>
