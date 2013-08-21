#	$NetBSD: Makefile,v 1.13 2013/02/21 08:20:19 martin Exp $

.include <bsd.own.mk>

# DEFAULTVT for all arches that are able to switch virtual consoles
# while in X (currently this needs WSDISPLAY_COMPAT_USL and
# WSDISPLAY_COMPAT_PCVT, this might change soon)
.if	   ${MACHINE_ARCH} == "x86_64" \
	|| ${MACHINE_ARCH} == "cats" \
	|| ${MACHINE_ARCH} == "hp700" \
	|| ${MACHINE_ARCH} == "hpcmips" \
	|| ${MACHINE_ARCH} == "hpcsh" \
	|| ${MACHINE_ARCH} == "i386" \
	|| ${MACHINE_ARCH} == "netwinder" \
	|| ${MACHINE_ARCH} == "shark" \
	|| ${MACHINE_ARCH} == "sparc" \
	|| ${MACHINE_ARCH} == "sparc64" \
	|| ${MACHINE_ARCH} == "zaurus"
DEFAULTVT=vt05
.else
DEFAULTVT=	# defined empty
.endif

UUDECODE_FILES=	NetBSD-flag.png
CPPSCRIPTS=	Xresources Xreset Xservers.ws Xsession Xstartup xdm-config 

CPPSCRIPTFLAGS_Xresources=	-DXPM -DBITMAPDIR=${X11INCDIR}/X11/pixmaps \
				-DXDM_PIXMAP=NetBSD.xpm \
				-DXDM_BWPIXMAP=NetBSD-bw.xpm
CPPSCRIPTFLAGS_Xreset=		-DBINDIR=${X11BINDIR} -DXDMCONFIGDIR=${XDMDIR}
CPPSCRIPTFLAGS_Xservers.ws=	-DBINDIR=${X11BINDIR} -DDEFAULTVT=${DEFAULTVT}
CPPSCRIPTFLAGS_Xsession=	-DBINDIR=${X11BINDIR} \
				-DMKTEMP_COMMAND=/usr/bin/mktemp \
				-DSHELL_CMD=/bin/sh
CPPSCRIPTFLAGS_Xstartup=	-DBINDIR=${X11BINDIR} -DXDMCONFIGDIR=${XDMDIR} \
				-DUTMP_FILE=${XDMPIDDIR}/utmp \
				-DWTMP_FILE=${XDMLOGDIR}/wtmp
CPPSCRIPTFLAGS_xdm-config=	-DXDMDIR=${XDMDIR} \
				-DXDMXAUTHDIR=${XDMVARDIR} \
				-DXDMLOGDIR=${XDMLOGDIR} \
				-DXDMPIDDIR=${XDMPIDDIR} \
				-DXPM \
				-DSU="su -m"

CONFIGFILES=		GiveConsole TakeConsole \
			Xaccess Xservers.fs Xsession Xsetup_0 Xwilling \
			${CPPSCRIPTS} 
FILES=			xorg-bw.xpm xorg.xpm \
			NetBSD-bw.xpm NetBSD-inv.xpm NetBSD.xpm	\
			NetBSD-flag.png NetBSD-flag1.xpm NetBSD-flag2.xpm
FILESDIR=		${XDMDIR}
FILESDIR_xorg.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_xorg-bw.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD-flag.png=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD-flag1.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD-flag2.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD-bw.xpm=	${X11INCDIR}/X11/pixmaps
FILESDIR_NetBSD-inv.xpm=${X11INCDIR}/X11/pixmaps
FILESMODE_GiveConsole=	${BINMODE}
FILESMODE_TakeConsole=	${BINMODE}
FILESMODE_Xreset=	${BINMODE}
FILESMODE_Xsetup_0=	${BINMODE}
FILESMODE_Xsession=	${BINMODE}
FILESMODE_Xstartup=	${BINMODE}
FILESMODE_Xwilling=	${BINMODE}

CONFIGSYMLINKS=		Xservers.ws ${XDMDIR}/Xservers

.PATH:		${X11SRCDIR.xdm}/config

.include "../Makefile.xdm"

CPPSCRIPTFLAGS_xdm-config+=	-DCHOOSERPATH=${XDMCHOOSERPATH}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
