#	$NetBSD: Makefile,v 1.13 2013/06/03 20:23:39 mrg Exp $

# LSC: Skipped for now
# xdm : uses network structures we still do not have (struct ifconf, struct ifreq)
# xlsclients: depends on xcb-atom from xcb-utils
SUBDIR=	appres bdftopcf bdftruncate beforelight bitmap ccmakedep \
	cleanlinks cxpm editres \
	fc-cache fc-cat fc-list fc-match fc-pattern fc-query fc-scan \
	fc-validate \
	fonttosfnt fslsfonts fstobdf glxgears glxinfo \
	iceauth ico imake listres lndir luit makedepend \
	makeg makestrs mergelib mkdirhier mkfontdir \
	mkfontscale mkhtmlindex oclock proxymngr revpath \
	rgb setxkbmap smproxy sxpm \
	twm ucs2any viewres x11perf xauth xbiff xcalc \
	xclipboard xclock xcmsdb xconsole xcutsel xditview  \
	xdpyinfo xedit xev xeyes xf86dga xfd xfindproxy xfontsel \
	xfs xfsinfo xfwp xgamma xgc xhost xinit xinput xkbcomp \
	xkbevd xkbprint xkbutils xkill xload xlogo xlsatoms  \
	xlsfonts xmag xman xmessage xmh xmkmf xmlwf xmodmap xmore \
	xprop xrandr xrdb xrefresh xset xsetmode xsetpointer \
	xsetroot xsm xstdcmap xterm xtrap xvidtune \
	xvinfo xwd xwininfo xwud xdriinfo sessreg

SUBDIR+=ssh-askpass
SUBDIR+=xsetwallpaper

# bring back from xfree:
#	lndir

.include <bsd.subdir.mk>
