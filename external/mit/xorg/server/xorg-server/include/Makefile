#	$NetBSD: Makefile,v 1.10 2011/08/02 07:56:33 mrg Exp $

.include <bsd.own.mk>

XORGSRC=	${X11SRCDIR.xorg-server}

XORGINC_SUBDIR=	Xext dbe exa fb include mi \
		miext/damage miext/shadow randr render xkb record
XF86_SUBDIR=	common ddc dixmods/extmod dri fbdevhw i2c int10 modes \
		os-support os-support/bus parser rac ramdac shadowfb \
		vbe vgahw xaa xf1bpp xf4bpp xf8_16bpp xf8_32bpp dri2

.for _dir in ${XORGINC_SUBDIR}
.PATH: ${XORGSRC}/${_dir}
.endfor

.for _dir in ${XF86_SUBDIR}
.PATH: ${XORGSRC}/hw/xfree86/${_dir}
.endfor

.PATH: ${XORGSRC}/../include

INCSDIR=${X11INCDIR}/xorg
INCS=	BT.h IBM.h TI.h XIstubs.h Xprintf.h \
	bt829.h \
	callback.h closestr.h closure.h colormap.h \
	colormapst.h compiler.h cursor.h cursorstr.h damage.h damagestr.h \
	dbestruct.h dgaproc.h dix.h dixevents.h dixfont.h dixfontstr.h \
	dixgrabs.h dixstruct.h dri.h dristruct.h edid.h exa.h exevents.h \
	eventconvert.h events.h eventstr.h list.h \
	extension.h extinit.h extnsionst.h fb.h fbdevhw.h fboverlay.h \
	fbrop.h fi1236.h fourcc.h gc.h gcstruct.h geext.h globals.h \
	glyphstr.h i2c_def.h input.h inputstr.h inpututils.h \
	mi.h mibstore.h micmap.h micoord.h mifillarc.h mifpoly.h \
	migc.h miline.h mipict.h mipointer.h mipointrst.h misc.h miscstruct.h \
	mispans.h mistruct.h miwideline.h mizerarc.h msp3430.h opaque.h \
	os.h picture.h picturestr.h pixmap.h pixmapstr.h property.h \
	protocol-versions.h \
	propertyst.h randrstr.h region.h regionstr.h resource.h \
	rgb.h sarea.h screenint.h scrnintstr.h selection.h servermd.h \
	shadow.h shadowfb.h site.h swaprep.h swapreq.h tda8425.h tda9850.h \
	tda9885.h uda1380.h validate.h vbe.h vbeModes.h vgaHW.h \
	window.h windowstr.h xaa.h xaalocal.h xaarop.h \
	xkbDflts.h xkb.h set.h \
	xf86.h xf86Crtc.h xf86Cursor.h xf86DDC.h \
	xf86Modes.h xf86Module.h \
	xf86Opt.h xf86Optrec.h xf86Parser.h xf86Pci.h xf86PciInfo.h \
	xf86Priv.h xf86Privstr.h xf86RamDac.h xf86RandR12.h \
	xf86Rename.h xf86Xinput.h xf86_OSlib.h \
	xf86_OSproc.h xf86cmap.h xf86fbman.h \
	xf86i2c.h xf86int10.h xf86sbusBus.h xf86str.h xf86xv.h \
	xkbstr.h xkbrules.h xserver-properties.h \
	dixaccess.h privates.h ptrveloc.h registry.h xkbfile.h \
	xf86xvmc.h xf86xvpriv.h xisb.h xorg-server.h xorgVersion.h xvdix.h \
	xvmcext.h hotplug.h xkbsrv.h dix-config.h dri2.h rrtransform.h \
	xsha1.h version-config.h

.include <bsd.x11.mk>
.include <bsd.prog.mk>
