#	$NetBSD: Makefile,v 1.4 2013/06/03 20:23:39 mrg Exp $

.include <bsd.own.mk>

.PATH:	${X11SRCDIR.fontconfig}/conf.d
CONFIGFILES= \
	10-autohint.conf \
	10-no-sub-pixel.conf \
	10-sub-pixel-bgr.conf \
	10-sub-pixel-rgb.conf \
	10-sub-pixel-vbgr.conf \
	10-sub-pixel-vrgb.conf \
	10-scale-bitmap-fonts.conf \
	10-unhinted.conf \
	11-lcdfilter-default.conf \
	11-lcdfilter-legacy.conf \
	11-lcdfilter-light.conf \
	20-unhint-small-vera.conf \
	25-unhint-nonlatin.conf \
	30-metric-aliases.conf \
	30-urw-aliases.conf \
	40-nonlatin.conf \
	45-latin.conf \
	49-sansserif.conf \
	50-user.conf \
	51-local.conf \
	60-latin.conf \
	65-fonts-persian.conf \
	65-khmer.conf \
	65-nonlatin.conf \
	69-unifont.conf \
	70-no-bitmaps.conf \
	70-yes-bitmaps.conf \
	80-delicious.conf \
	90-synthetic.conf

FILESDIR= \
	/etc/fonts/conf.avail

.include <bsd.x11.mk>
.include <bsd.prog.mk>
