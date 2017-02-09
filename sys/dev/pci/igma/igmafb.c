/*	$NetBSD: igmafb.c,v 1.1 2014/01/21 14:52:07 mlelstv Exp $	*/

/*
 * Copyright (c) 2012 Michael van Elst
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Intel Graphic Media Accelerator
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igmafb.c,v 1.1 2014/01/21 14:52:07 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/kmem.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/videomode/videomode.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/pci/wsdisplay_pci.h>

#include <dev/pci/igmareg.h>
#include <dev/pci/igmavar.h>

#include "opt_voyagerfb.h"

struct igmafb_softc {
	device_t		sc_dev;

	int 			sc_width;
	int 			sc_height;
	int 			sc_depth;
	int 			sc_stride;
	void			*sc_fbaddr;
	bus_size_t		sc_fbsize;
	struct vcons_screen	sc_console_screen;
	struct wsscreen_descr	sc_defaultscreen_descr;
	const struct wsscreen_descr	*sc_screens[1];
	struct wsscreen_list	sc_screenlist;
	struct vcons_data	vd;

	struct igma_chip	sc_chip;
	void			*sc_vga_save;

	int			sc_backlight;
	int			sc_brightness;
	int			sc_brightness_max;
};

static int igmafb_match(device_t, cfdata_t, void *);
static void igmafb_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(igmafb, sizeof(struct igmafb_softc),
    igmafb_match, igmafb_attach, NULL, NULL);

static int igmafb_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t igmafb_mmap(void *, void *, off_t, int);
static void igmafb_pollc(void *v, int);

static /*const*/ struct wsdisplay_accessops igmafb_accessops = {
	igmafb_ioctl,
	igmafb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL,	/* load_font */
	igmafb_pollc,	/* pollc */
	NULL	/* scroll */
};

static void igmafb_init_screen(void *, struct vcons_screen *, int, long *);
static void igmafb_guess_size(struct igmafb_softc *, int *, int*);
static void igmafb_set_mode(struct igmafb_softc *, bool);

static void igmafb_planestart_quirk(struct igmafb_softc *);
static void igmafb_pfitdisable_quirk(struct igmafb_softc *);

static void igmafb_get_brightness_max(struct igmafb_softc *, int *);
static void igmafb_get_brightness(struct igmafb_softc *, int *);
static void igmafb_set_brightness(struct igmafb_softc *, int);

static int
igmafb_match(device_t parent, cfdata_t match, void *aux)
{
	struct igma_attach_args *iaa = (struct igma_attach_args *)aux;

	if (strcmp(iaa->iaa_name, "igmafb") == 0) return 100;
	return 0;
}

static void
igmafb_attach(device_t parent, device_t self, void *aux)
{
	struct igmafb_softc *sc = device_private(self);
	struct igma_attach_args *iaa = (struct igma_attach_args *)aux;
	struct rasops_info *ri;
	prop_dictionary_t dict;
	bool is_console;
	unsigned long defattr;
	struct wsemuldisplaydev_attach_args waa;

	sc->sc_dev = self;

	aprint_normal("\n");

	dict = device_properties(self);
	prop_dictionary_get_bool(dict, "is_console", &is_console);
	if (iaa->iaa_console)
		is_console = true;

	sc->sc_chip = iaa->iaa_chip;

	sc->sc_fbaddr = bus_space_vaddr(sc->sc_chip.gmt, sc->sc_chip.gmh);
	sc->sc_fbsize = 16 * 1024 * 1024;

	igmafb_guess_size(sc, &sc->sc_width, &sc->sc_height);
	sc->sc_depth = 32;
	sc->sc_stride = (sc->sc_width*4 + 511)/512*512;

	aprint_normal("%s: %d x %d, %d bit, stride %d\n", device_xname(self),
		sc->sc_width, sc->sc_height, sc->sc_depth, sc->sc_stride);

	aprint_normal("%s: %d MB video memory at 0x%p\n", device_xname(self),
		(int)sc->sc_fbsize >> 20, (void *)sc->sc_chip.gmb);

	sc->sc_vga_save = kmem_alloc(256*1024, KM_SLEEP);

	igmafb_get_brightness(sc, &sc->sc_brightness);
	igmafb_get_brightness_max(sc, &sc->sc_brightness_max);
	sc->sc_backlight = sc->sc_brightness != 0;

	sc->sc_defaultscreen_descr = (struct wsscreen_descr){
		"default",
		0, 0,
		NULL,
		8, 16,
		WSSCREEN_WSCOLORS | WSSCREEN_HILIT,
		NULL
	};
	sc->sc_screens[0] = &sc->sc_defaultscreen_descr;
	sc->sc_screenlist = (struct wsscreen_list){1, sc->sc_screens};

	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
		&igmafb_accessops);
	sc->vd.init_screen = igmafb_init_screen;

	/* enable hardware display */
	igmafb_set_mode(sc, true);

	ri = &sc->sc_console_screen.scr_ri;

	if (is_console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
			&defattr);

		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC
			| VCONS_NO_COPYROWS | VCONS_NO_COPYCOLS;
		vcons_redraw_screen(&sc->sc_console_screen);

		sc->sc_defaultscreen_descr.textops = &ri->ri_ops;
		sc->sc_defaultscreen_descr.capabilities = ri->ri_caps;
		sc->sc_defaultscreen_descr.nrows = ri->ri_rows;
		sc->sc_defaultscreen_descr.ncols = ri->ri_cols;

		wsdisplay_cnattach(&sc->sc_defaultscreen_descr, ri, 0, 0,
			defattr);
		vcons_replay_msgbuf(&sc->sc_console_screen);
	} else {
		if (sc->sc_console_screen.scr_ri.ri_rows == 0) {
			/* do some minimal setup to avoid weirdness later */
			vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
				&defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	}

	waa.console = is_console;
	waa.scrdata = &sc->sc_screenlist;
	waa.accessops = &igmafb_accessops;
	waa.accesscookie = &sc->vd;

	config_found(sc->sc_dev, &waa, wsemuldisplaydevprint);
}

/*
 * wsdisplay accessops
 */

static int
igmafb_ioctl(void *v, void *vs, u_long cmd, void *data, int flags,
    struct lwp *l)
{
	struct vcons_data *vd = v;
	struct igmafb_softc *sc = vd->cookie;
	struct wsdisplay_fbinfo *wdf;
	struct vcons_screen *ms = vd->active;
	struct wsdisplayio_fbinfo *fbi;
	struct wsdisplay_param *param;
	int val;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;
	case WSDISPLAYIO_GINFO:
		if (ms == NULL)
			return ENODEV;
		wdf = data;
		wdf->width  = ms->scr_ri.ri_width;
		wdf->height = ms->scr_ri.ri_height;
		wdf->depth  = ms->scr_ri.ri_depth;
		wdf->cmsize = 256; /* XXX */
		return 0;
	case WSDISPLAYIO_LINEBYTES:
		if (ms == NULL)
			return ENODEV;
		*(u_int *)data = ms->scr_ri.ri_stride;
		return 0;
	case WSDISPLAYIO_GET_FBINFO:
		fbi = data;
		return wsdisplayio_get_fbinfo(&ms->scr_ri, fbi);
	case WSDISPLAYIO_SVIDEO:
		val = (*(u_int *)data) != WSDISPLAYIO_VIDEO_OFF;
		sc->sc_backlight = val;
		if (val)
			igmafb_set_brightness(sc, sc->sc_brightness);
		else
			igmafb_set_brightness(sc, 0);
		return 0;
	case WSDISPLAYIO_GETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			param->min = 0;
			param->max = 255;
			if (sc->sc_backlight)
				igmafb_get_brightness(sc, &val);
			else
				val = sc->sc_brightness;
			val = val * 255 / sc->sc_brightness_max;
			param->curval = val;
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			param->min = 0;
			param->max = 1;
			param->curval = sc->sc_backlight;
			return 0;
		}
		return EPASSTHROUGH;
	case WSDISPLAYIO_SETPARAM:
		param = (struct wsdisplay_param *)data;
		switch (param->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			val = param->curval;
			if (val < 0)
				val = 0;
			if (val > 255)
				val = 255;
			val = val * sc->sc_brightness_max / 255;
			sc->sc_brightness = val;
			if (sc->sc_backlight)
				igmafb_set_brightness(sc, val);
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			val = param->curval;
			sc->sc_backlight = val;
			if (val)
				igmafb_set_brightness(sc, sc->sc_brightness);
			else
				igmafb_set_brightness(sc, 0);
			return 0;
		}
		return EPASSTHROUGH;
	}

	return EPASSTHROUGH;
}

static paddr_t
igmafb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct igmafb_softc *sc = vd->cookie;

	if ((offset & PAGE_MASK) != 0)
		return -1;

	if (offset < 0 || offset >= sc->sc_fbsize)
		return -1;

	return bus_space_mmap(sc->sc_chip.gmt, sc->sc_chip.gmb, offset, prot,
		BUS_SPACE_MAP_LINEAR);
}

static void
igmafb_pollc(void *v, int on)
{
	struct vcons_data *vd = v;
	struct igmafb_softc *sc = vd->cookie;

	if (sc == NULL)
		return;
	if (sc->sc_console_screen.scr_vd == NULL)
		return;

	if (on)
		vcons_enable_polling(&sc->vd);
	else
		vcons_disable_polling(&sc->vd);
}

static void
igmafb_init_screen(void *cookie, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct igmafb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	memset(ri, 0, sizeof(struct rasops_info));

	ri->ri_depth = sc->sc_depth;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_stride;
	ri->ri_flg = RI_CENTER | RI_FULLCLEAR;

	ri->ri_bits = (char *)sc->sc_fbaddr;

	if (existing) {
		ri->ri_flg |= RI_CLEAR;
	}

	switch (sc->sc_depth) {
	case 32:
		ri->ri_rnum = 8;
		ri->ri_gnum = 8;
		ri->ri_bnum = 8;
		ri->ri_rpos = 16;
		ri->ri_gpos = 8;
		ri->ri_bpos = 0;
		break;
	}

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;

	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
		sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;
}

static void
igmafb_guess_size(struct igmafb_softc *sc, int *widthp, int *heightp)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	int pipe = cd->use_pipe;
	u_int32_t r;

	r = co->read_reg(cd, PIPE_HTOTAL(pipe));
	*widthp = PIPE_HTOTAL_GET_ACTIVE(r);
	r = co->read_reg(cd, PIPE_VTOTAL(pipe));
	*heightp = PIPE_VTOTAL_GET_ACTIVE(r);

	aprint_normal("%s: vga active size %d x %d\n",
		device_xname(sc->sc_dev),
		*widthp, *heightp);

	if (*widthp < 640 || *heightp < 400) {
		r = co->read_reg(cd, PF_WINSZ(pipe));
		*widthp  = PF_WINSZ_GET_WIDTH(r);
		*heightp = PF_WINSZ_GET_HEIGHT(r);

		aprint_normal("%s: window size %d x %d\n",
			device_xname(sc->sc_dev),
			*widthp, *heightp);
	}

	if (*widthp  < 640) *widthp  = 640;
	if (*heightp < 400) *heightp = 400;
}

static void
igmafb_set_mode(struct igmafb_softc *sc, bool enable)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	int pipe = cd->use_pipe;
	u_int32_t r;
	u_int8_t b;
	int i;

	if (enable) {
		/* disable VGA machinery */
		b = co->read_vga(cd, 0x01);
		co->write_vga(cd, 0x01, b | 0x20);

		/* disable VGA compatible display */
		r = co->read_reg(cd, sc->sc_chip.vga_cntrl);
		co->write_reg(cd, sc->sc_chip.vga_cntrl, r | VGA_CNTRL_DISABLE);

		/* save VGA memory */
		memcpy(sc->sc_vga_save, sc->sc_fbaddr, 256*1024);

		/* configure panel fitter */
		co->write_reg(cd, PF_WINPOS(pipe),
			PF_WINPOS_VAL(0, 0));
		co->write_reg(cd, PF_WINSZ(pipe),
			PF_WINSZ_VAL(sc->sc_width, sc->sc_height));

		/* pipe size */
		co->write_reg(cd, PIPE_SRCSZ(pipe),
			PIPE_SRCSZ_VAL(sc->sc_width, sc->sc_height));

		/* enable pipe */
		co->write_reg(cd, PIPE_CONF(pipe),
			PIPE_CONF_ENABLE | PIPE_CONF_8BPP);

		/* configure planes */
		r = co->read_reg(cd, PRI_CTRL(pipe));
		r &= ~(PRI_CTRL_PIXFMTMSK | PRI_CTRL_TILED);
		r |= PRI_CTRL_ENABLE | PRI_CTRL_BGR;
		co->write_reg(cd, PRI_CTRL(pipe), r | cd->pri_cntrl);
		co->write_reg(cd, PRI_LINOFF(pipe), 0);
		co->write_reg(cd, PRI_STRIDE(pipe), sc->sc_stride);
		co->write_reg(cd, PRI_SURF(pipe), 0);
		co->write_reg(cd, PRI_TILEOFF(pipe), 0);

		if (cd->quirks & IGMA_PLANESTART_QUIRK)
			igmafb_planestart_quirk(sc);

		if (cd->quirks & IGMA_PFITDISABLE_QUIRK)
			igmafb_pfitdisable_quirk(sc);
	} else {
		/* disable planes */
		co->write_reg(cd, PRI_CTRL(pipe), 0 | cd->pri_cntrl);
		co->write_reg(cd, PRI_LINOFF(pipe), 0);
		co->write_reg(cd, PRI_STRIDE(pipe), 2560);
		co->write_reg(cd, PRI_SURF(pipe), 0);
		co->write_reg(cd, PRI_TILEOFF(pipe), 0);

		/* pipe size */
		co->write_reg(cd, PIPE_SRCSZ(pipe),
			PIPE_SRCSZ_VAL(720,400));

		/* disable pipe */
		co->write_reg(cd, PIPE_CONF(pipe), 0);
		for (i=0; i<10; ++i) {
			delay(10);
			if ((co->read_reg(cd, PIPE_CONF(pipe)) & PIPE_CONF_STATE) == 0)
				break;
		}

		/* workaround before enabling VGA */
		r = co->read_reg(cd, 0x42000);
		co->write_reg(cd, 0x42000, (r & 0x1fffffff) | 0xa0000000);
		r = co->read_reg(cd, 0x42004);
		co->write_reg(cd, 0x42004, (r & 0xfbffffff) | 0x00000000);

		/* configure panel fitter */
		co->write_reg(cd, PF_WINPOS(pipe),
			PF_WINPOS_VAL(0, 0));
		co->write_reg(cd, PF_WINSZ(pipe),
			PF_WINSZ_VAL(sc->sc_width, sc->sc_height));

		/* enable VGA compatible display */
		r = co->read_reg(cd, sc->sc_chip.vga_cntrl);
		co->write_reg(cd, sc->sc_chip.vga_cntrl, r & ~VGA_CNTRL_DISABLE);

		/* enable VGA machinery */
		b = co->read_vga(cd, 0x01);
		co->write_vga(cd, 0x01, b & ~0x20);

		/* restore VGA memory */
		memcpy(sc->sc_fbaddr, sc->sc_vga_save, 256*1024);

		/* enable pipe again */
		co->write_reg(cd, PIPE_CONF(pipe),
			PIPE_CONF_ENABLE | PIPE_CONF_6BPP | PIPE_CONF_DITHER);
	}
}

static void
igmafb_planestart_quirk(struct igmafb_softc *sc)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	int pipe = cd->use_pipe;
	u_int32_t cntrl, fwbcl;

	/* disable self refresh */
	fwbcl = co->read_reg(cd, FW_BLC_SELF);
	co->write_reg(cd, FW_BLC_SELF, fwbcl & ~FW_BLC_SELF_EN);

	cntrl = co->read_reg(cd, CUR_CNTR(pipe));
	co->write_reg(cd, CUR_CNTR(pipe), 1<<5 | 0x07);

	/* "wait for vblank" */
	delay(40000);

	co->write_reg(cd, CUR_CNTR(pipe), cntrl);
	co->write_reg(cd, CUR_BASE(pipe),
		co->read_reg(cd, CUR_BASE(pipe)));

	co->write_reg(cd, FW_BLC_SELF, fwbcl);
}

static void
igmafb_pfitdisable_quirk(struct igmafb_softc *sc)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	u_int32_t r;

	/* disable i965 panel fitter */
	r = co->read_reg(cd, PF_CTRL_I965);
	co->write_reg(cd, PF_CTRL_I965, r & ~PF_ENABLE);
}

static void
igmafb_get_brightness_max(struct igmafb_softc *sc, int *valp)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	u_int32_t r, f;

	r = co->read_reg(cd, cd->backlight_cntrl);
	f = BACKLIGHT_GET_FREQ(r);
	if (f == 0) {
		r = co->read_reg(cd, RAWCLK_FREQ);
		f = r * 1000000 / (200 * 128);
		if (f == 0 || f > 32767)
			f = 125 * 100000 / (200 * 128);
	}

	*valp = f;
}

static void
igmafb_get_brightness(struct igmafb_softc *sc, int *valp)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	u_int32_t r, v;

	r = co->read_reg(cd, cd->backlight_cntrl);
	v = BACKLIGHT_GET_CYCLE(r);
	*valp = v;
}

static void
igmafb_set_brightness(struct igmafb_softc *sc, int val)
{
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	u_int32_t r, f, l;

	r = co->read_reg(cd, cd->backlight_cntrl);
	f = BACKLIGHT_GET_FREQ(r);
	l = BACKLIGHT_GET_LEGACY(r);

	co->write_reg(cd, cd->backlight_cntrl,
		BACKLIGHT_VAL(f,l,val));
}

