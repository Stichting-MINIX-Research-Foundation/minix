/*	$NetBSD: igsfb.c,v 1.52 2012/01/11 20:41:28 macallan Exp $ */

/*
 * Copyright (c) 2002, 2003 Valeriy E. Ushakov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Integraphics Systems IGA 168x and CyberPro series.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igsfb.c,v 1.52 2012/01/11 20:41:28 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>

#include <sys/bus.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>

#include <dev/wscons/wsdisplay_vconsvar.h>

#include <dev/ic/igsfbreg.h>
#include <dev/ic/igsfbvar.h>


struct igsfb_devconfig igsfb_console_dc = {
	.dc_mmap = NULL,
	.dc_modestring = "",
};

/*
 * wsscreen
 */

/* filled from rasops_info in igsfb_init_wsdisplay */
static struct wsscreen_descr igsfb_stdscreen = {
	.name = "std",
};

static const struct wsscreen_descr *_igsfb_scrlist[] = {
	&igsfb_stdscreen,
};

static const struct wsscreen_list igsfb_screenlist = {
	.nscreens = sizeof(_igsfb_scrlist) / sizeof(_igsfb_scrlist[0]),
	.screens = _igsfb_scrlist,
};


/*
 * wsdisplay_accessops
 */

static int	igsfb_ioctl(void *, void *, u_long, void *, int, struct lwp *);
static paddr_t	igsfb_mmap(void *, void *, off_t, int);

static struct wsdisplay_accessops igsfb_accessops = {
	.ioctl = igsfb_ioctl,
	.mmap = igsfb_mmap,
};


/*
 * acceleration
 */
static int	igsfb_make_text_cursor(struct igsfb_devconfig *,
				       struct vcons_screen *);
static void	igsfb_accel_cursor(void *, int, int, int);

static int	igsfb_accel_wait(struct igsfb_devconfig *);
static void	igsfb_accel_fill(struct igsfb_devconfig *,
				 uint32_t, uint32_t, uint16_t, uint16_t);
static void	igsfb_accel_copy(struct igsfb_devconfig *,
				 uint32_t, uint32_t, uint16_t, uint16_t);

static void	igsfb_accel_copycols(void *, int, int, int, int);
static void	igsfb_accel_erasecols(void *, int, int, int, long);
static void	igsfb_accel_copyrows(void *, int, int, int);
static void	igsfb_accel_eraserows(void *, int, int, long);
static void	igsfb_accel_putchar(void *, int, int, u_int, long);


/*
 * internal functions
 */
static int	igsfb_init_video(struct igsfb_devconfig *);
static void	igsfb_init_cmap(struct igsfb_devconfig *);
static uint16_t	igsfb_spread_bits_8(uint8_t);
static void	igsfb_init_bit_table(struct igsfb_devconfig *);
static void	igsfb_init_wsdisplay(void *, struct vcons_screen *, int,
				     long *);


static void	igsfb_blank_screen(struct igsfb_devconfig *, int);
static int	igsfb_get_cmap(struct igsfb_devconfig *,
			       struct wsdisplay_cmap *);
static int	igsfb_set_cmap(struct igsfb_devconfig *,
			       const struct wsdisplay_cmap *);
static void	igsfb_update_cmap(struct igsfb_devconfig *, u_int, u_int);
static void	igsfb_set_curpos(struct igsfb_devconfig *,
				 const struct wsdisplay_curpos *);
static void	igsfb_update_curpos(struct igsfb_devconfig *);
static int	igsfb_get_cursor(struct igsfb_devconfig *,
				 struct wsdisplay_cursor *);
static int	igsfb_set_cursor(struct igsfb_devconfig *,
				 const struct wsdisplay_cursor *);
static void	igsfb_update_cursor(struct igsfb_devconfig *, u_int);
static void	igsfb_convert_cursor_data(struct igsfb_devconfig *,
					  u_int, u_int);


int
igsfb_cnattach_subr(struct igsfb_devconfig *dc)
{
	struct rasops_info *ri;
	long defattr;

	KASSERT(dc == &igsfb_console_dc);

	igsfb_init_video(dc);
	dc->dc_vd.active = NULL;
	igsfb_init_wsdisplay(dc, &dc->dc_console, 1, &defattr);

	ri = &dc->dc_console.scr_ri;
	ri->ri_hw = &dc->dc_console;
	dc->dc_console.scr_cookie = dc;

	(*ri->ri_ops.allocattr)(ri,
				WS_DEFAULT_FG, /* fg */
				WS_DEFAULT_BG, /* bg */
				0,           /* wsattrs */
				&defattr);

	wsdisplay_cnattach(&igsfb_stdscreen,
			   ri,   /* emulcookie */
			   0, 0, /* cursor position */
			   defattr);
	return 0;
}


/*
 * Finish off the attach.  Bus specific attach method should have
 * enabled io and memory accesses and mapped io (and cop?) registers.
 */
void
igsfb_attach_subr(struct igsfb_softc *sc, int isconsole)
{
	struct igsfb_devconfig *dc = sc->sc_dc;
	struct wsemuldisplaydev_attach_args waa;
	struct rasops_info *ri;
	long defattr;

	KASSERT(dc != NULL);

	if (!isconsole) {
		igsfb_init_video(dc);
	}

	vcons_init(&dc->dc_vd, dc, &igsfb_stdscreen, &igsfb_accessops);
	dc->dc_vd.init_screen = igsfb_init_wsdisplay;

	vcons_init_screen(&dc->dc_vd, &dc->dc_console, 1, &defattr);
	dc->dc_console.scr_flags |= VCONS_SCREEN_IS_STATIC;

	printf("%s: %dMB, %s%dx%d, %dbpp\n",
	       device_xname(sc->sc_dev),
	       (uint32_t)(dc->dc_vmemsz >> 20),
	       (dc->dc_hwflags & IGSFB_HW_BSWAP)
		   ? (dc->dc_hwflags & IGSFB_HW_BE_SELECT)
		       ? "hardware bswap, " : "software bswap, "
		   : "",
	       dc->dc_width, dc->dc_height, dc->dc_depth);
	printf("%s: using %dbpp for X\n", device_xname(sc->sc_dev),
	       dc->dc_maxdepth);
	ri = &dc->dc_console.scr_ri;
	ri->ri_ops.eraserows(ri, 0, ri->ri_rows, defattr);

	if (isconsole)
		vcons_replay_msgbuf(&dc->dc_console);

	/* attach wsdisplay */
	waa.console = isconsole;
	waa.scrdata = &igsfb_screenlist;
	waa.accessops = &igsfb_accessops;
	waa.accesscookie = &dc->dc_vd;

	config_found(sc->sc_dev, &waa, wsemuldisplaydevprint);
}


static int
igsfb_init_video(struct igsfb_devconfig *dc)
{
	bus_space_handle_t tmph;
	uint8_t *p;
	int need_bswap;
	bus_addr_t fbaddr, craddr;
	off_t croffset;
	uint8_t busctl, curctl;
	void *va;

	/* Total amount of video memory. */
	busctl = igs_ext_read(dc->dc_iot, dc->dc_ioh, IGS_EXT_BUS_CTL);
	if (busctl & 0x2)
		dc->dc_vmemsz = 4;
	else if (busctl & 0x1)
		dc->dc_vmemsz = 2;
	else
		dc->dc_vmemsz = 1;
	dc->dc_vmemsz <<= 20;	/* megabytes -> bytes */

	/*
	 * Check for endianness mismatch by writing a word at the end of
	 * the video memory (off-screen) and reading it back byte-by-byte.
	 */
	if (bus_space_map(dc->dc_memt,
			  dc->dc_memaddr + dc->dc_vmemsz - sizeof(uint32_t),
			  sizeof(uint32_t),
			  dc->dc_memflags | BUS_SPACE_MAP_LINEAR,
			  &tmph) != 0)
	{
		printf("unable to map video memory for endianness test\n");
		return 1;
	}

	p = bus_space_vaddr(dc->dc_memt, tmph);
#if BYTE_ORDER == BIG_ENDIAN
	*((uint32_t *)p) = 0x12345678;
#else
	*((uint32_t *)p) = 0x78563412;
#endif
	if (p[0] == 0x12 && p[1] == 0x34 && p[2] == 0x56 && p[3] == 0x78)
		need_bswap = 0;
	else
		need_bswap = 1;

	bus_space_unmap(dc->dc_memt, tmph, sizeof(uint32_t));

	/*
	 * On CyberPro we can use magic bswap bit in linear address.
	 */
	fbaddr = dc->dc_memaddr;
	if (need_bswap) {
		dc->dc_hwflags |= IGSFB_HW_BSWAP;
		if (dc->dc_id >= 0x2000) {
			dc->dc_hwflags |= IGSFB_HW_BE_SELECT;
			fbaddr |= IGS_MEM_BE_SELECT;
		}
	}

	igsfb_hw_setup(dc);

	/*
	 * Don't map in all N megs, just the amount we need for the wsscreen.
	 */
	dc->dc_fbsz = dc->dc_stride * dc->dc_height;
	if (bus_space_map(dc->dc_memt, fbaddr, dc->dc_fbsz,
			  dc->dc_memflags | BUS_SPACE_MAP_LINEAR,
			  &dc->dc_fbh) != 0)
	{
		bus_space_unmap(dc->dc_iot, dc->dc_ioh, IGS_REG_SIZE);
		printf("unable to map framebuffer\n");
		return 1;
	}

	igsfb_init_cmap(dc);

	/*
	 * 1KB for cursor sprite data at the very end of the video memory.
	 */
	croffset = dc->dc_vmemsz - IGS_CURSOR_DATA_SIZE;
	craddr = fbaddr + croffset;
	if (bus_space_map(dc->dc_memt, craddr, IGS_CURSOR_DATA_SIZE,
			  dc->dc_memflags | BUS_SPACE_MAP_LINEAR,
			  &dc->dc_crh) != 0)
	{
		bus_space_unmap(dc->dc_iot, dc->dc_ioh, IGS_REG_SIZE);
		bus_space_unmap(dc->dc_memt, dc->dc_fbh, dc->dc_fbsz);
		printf("unable to map cursor sprite region\n");
		return 1;
	}

	/*
	 * Tell the device where cursor sprite data are located in the
	 * linear space (it takes data offset in 1KB units).
	 */
	croffset >>= 10;	/* bytes -> kilobytes */
	igs_ext_write(dc->dc_iot, dc->dc_ioh,
		      IGS_EXT_SPRITE_DATA_LO, croffset & 0xff);
	igs_ext_write(dc->dc_iot, dc->dc_ioh,
		      IGS_EXT_SPRITE_DATA_HI, (croffset >> 8) & 0xf);

	/* init the bit expansion table for cursor sprite data conversion */
	igsfb_init_bit_table(dc);

	/* XXX: fill dc_cursor and use igsfb_update_cursor() instead? */
	memset(&dc->dc_cursor, 0, sizeof(struct igs_hwcursor));
	va = bus_space_vaddr(dc->dc_memt, dc->dc_crh);
	memset(va, /* transparent */ 0xaa, IGS_CURSOR_DATA_SIZE);

	curctl = igs_ext_read(dc->dc_iot, dc->dc_ioh, IGS_EXT_SPRITE_CTL);
	curctl |= IGS_EXT_SPRITE_64x64;
	curctl &= ~IGS_EXT_SPRITE_VISIBLE;
	igs_ext_write(dc->dc_iot, dc->dc_ioh, IGS_EXT_SPRITE_CTL, curctl);
	dc->dc_curenb = 0;

	/*
	 * Map and init graphic coprocessor for accelerated rasops.
	 */
	if (dc->dc_id >= 0x2000) { /* XXX */
		if (bus_space_map(dc->dc_iot,
				  dc->dc_iobase + IGS_COP_BASE_B, IGS_COP_SIZE,
				  dc->dc_ioflags,
				  &dc->dc_coph) != 0)
		{
			printf("unable to map COP registers\n");
			return 1;
		}

		/* XXX: hardcoded 8bpp */
		bus_space_write_2(dc->dc_iot, dc->dc_coph,
				  IGS_COP_SRC_MAP_WIDTH_REG,
				  dc->dc_width - 1);
		bus_space_write_2(dc->dc_iot, dc->dc_coph,
				  IGS_COP_DST_MAP_WIDTH_REG,
				  dc->dc_width - 1);

		bus_space_write_1(dc->dc_iot, dc->dc_coph,
				  IGS_COP_MAP_FMT_REG,
				  IGS_COP_MAP_8BPP);
	}

	/* make sure screen is not blanked */
	dc->dc_blanked = 0;
	igsfb_blank_screen(dc, dc->dc_blanked);

	return 0;
}


static void
igsfb_init_cmap(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	const uint8_t *p;
	int i;

	p = rasops_cmap;	/* "ANSI" color map */

	/* init software copy */
	for (i = 0; i < IGS_CMAP_SIZE; ++i, p += 3) {
		dc->dc_cmap.r[i] = p[0];
		dc->dc_cmap.g[i] = p[1];
		dc->dc_cmap.b[i] = p[2];
	}

	/* propagate to the device */
	igsfb_update_cmap(dc, 0, IGS_CMAP_SIZE);

	/* set overscan color (XXX: use defattr's background?) */
	igs_ext_write(iot, ioh, IGS_EXT_OVERSCAN_RED,   0);
	igs_ext_write(iot, ioh, IGS_EXT_OVERSCAN_GREEN, 0);
	igs_ext_write(iot, ioh, IGS_EXT_OVERSCAN_BLUE,  0);
}


static void
igsfb_init_wsdisplay(void *cookie, struct vcons_screen *scr, int existing,
		     long *defattr)
{
	struct igsfb_devconfig *dc = cookie;
	struct rasops_info *ri = &scr->scr_ri;
	int wsfcookie;

	if (scr == &dc->dc_console) {
		if (ri->ri_flg == 0) {
			/* first time, need to set RI_NO_AUTO */
			ri->ri_flg |= RI_NO_AUTO;
		} else {
			/* clear it on 2nd run */
			ri->ri_flg &= ~RI_NO_AUTO;
		}
	}
	ri->ri_flg |= RI_CENTER | RI_FULLCLEAR;

	if (IGSFB_HW_SOFT_BSWAP(dc))
		ri->ri_flg |= RI_BSWAP;

	ri->ri_depth = dc->dc_depth;
	ri->ri_width = dc->dc_width;
	ri->ri_height = dc->dc_height;
	ri->ri_stride = dc->dc_stride;
	ri->ri_bits = bus_space_vaddr(dc->dc_memt, dc->dc_fbh);

	/*
	 * Initialize wsfont related stuff.
	 */
	wsfont_init();

	/* prefer gallant that is identical to the one the prom uses */
	wsfcookie = wsfont_find("Gallant", 12, 22, 0,
				WSDISPLAY_FONTORDER_L2R,
				WSDISPLAY_FONTORDER_L2R, WSFONT_FIND_BITMAP);
	if (wsfcookie <= 0) {
#ifdef DIAGNOSTIC
		printf("unable to find font Gallant 12x22\n");
#endif
		wsfcookie = wsfont_find(NULL, 0, 0, 0, /* any font at all? */
					WSDISPLAY_FONTORDER_L2R,
					WSDISPLAY_FONTORDER_L2R,
					WSFONT_FIND_BITMAP);
	}

	if (wsfcookie <= 0) {
		printf("unable to find any fonts\n");
		return;
	}

	if (wsfont_lock(wsfcookie, &ri->ri_font) != 0) {
		printf("unable to lock font\n");
		return;
	}
	ri->ri_wsfcookie = wsfcookie;


	/* XXX: TODO: compute term size based on font dimensions? */
	rasops_init(ri, 0, 0);
	rasops_reconfig(ri, ri->ri_height / ri->ri_font->fontheight,
	    ri->ri_width / ri->ri_font->fontwidth);


	/* use the sprite for the text mode cursor */
	igsfb_make_text_cursor(dc, scr);

	/* the cursor is "busy" while we are in the text mode */
	dc->dc_hwflags |= IGSFB_HW_TEXT_CURSOR;

	/* propagate sprite data to the device */
	igsfb_update_cursor(dc, WSDISPLAY_CURSOR_DOSHAPE);

	/* accelerated text cursor */
	ri->ri_ops.cursor = igsfb_accel_cursor;

	if (dc->dc_id >= 0x2000) { /* XXX */
		/* accelerated erase/copy */
		ri->ri_ops.copycols = igsfb_accel_copycols;
		ri->ri_ops.erasecols = igsfb_accel_erasecols;
		ri->ri_ops.copyrows = igsfb_accel_copyrows;
		ri->ri_ops.eraserows = igsfb_accel_eraserows;

		/* putchar hook to sync with the cop */
		dc->dc_ri_putchar = ri->ri_ops.putchar;
		ri->ri_ops.putchar = igsfb_accel_putchar;
	}

	igsfb_stdscreen.nrows = ri->ri_rows;
	igsfb_stdscreen.ncols = ri->ri_cols;
	igsfb_stdscreen.textops = &ri->ri_ops;
	igsfb_stdscreen.capabilities = ri->ri_caps;
}


/*
 * Init cursor data in dc_cursor for the accelerated text cursor.
 */
static int
igsfb_make_text_cursor(struct igsfb_devconfig *dc, struct vcons_screen *scr)
{
	struct rasops_info *ri = &scr->scr_ri;
	struct wsdisplay_font *f = ri->ri_font;
	uint16_t cc_scan[8];	/* one sprite scanline */
	uint16_t s;
	int w, i;

	KASSERT(f->fontwidth <= IGS_CURSOR_MAX_SIZE);
	KASSERT(f->fontheight <= IGS_CURSOR_MAX_SIZE);

	w = f->fontwidth;
	for (i = 0; i < f->stride; ++i) {
		if (w >= 8) {
			s = 0xffff; /* all inverted */
			w -= 8;
		} else {
			/* first w pixels inverted, the rest is transparent */
			s = ~(0x5555 << (w * 2));
			s = htole16(s);
			w = 0;
		}
		cc_scan[i] = s;
	}

	dc->dc_cursor.cc_size.x = f->fontwidth;
	dc->dc_cursor.cc_size.y = f->fontheight;

	dc->dc_cursor.cc_hot.x = 0;
	dc->dc_cursor.cc_hot.y = 0;

	/* init sprite array to be all transparent */
	memset(dc->dc_cursor.cc_sprite, 0xaa, IGS_CURSOR_DATA_SIZE);

	for (i = 0; i < f->fontheight; ++i)
		memcpy(&dc->dc_cursor.cc_sprite[i * 8],
		       cc_scan, f->stride * sizeof(uint16_t));

	return 0;
}


/*
 * Spread a byte (abcd.efgh) into two (0a0b.0c0d 0e0f.0g0h).
 * Helper function for igsfb_init_bit_table().
 */
static uint16_t
igsfb_spread_bits_8(uint8_t b)
{
	uint16_t s = b;

	s = ((s & 0x00f0) << 4) | (s & 0x000f);
	s = ((s & 0x0c0c) << 2) | (s & 0x0303);
	s = ((s & 0x2222) << 1) | (s & 0x1111);
	return s;
}


/*
 * Cursor sprite data are in 2bpp.  Incoming image/mask are in 1bpp.
 * Prebuild the table to expand 1bpp->2bpp, with bswapping if necessary.
 */
static void
igsfb_init_bit_table(struct igsfb_devconfig *dc)
{
	uint16_t *expand = dc->dc_bexpand;
	uint16_t s;
	u_int i;

	for (i = 0; i < 256; ++i) {
		s = igsfb_spread_bits_8(i);
		expand[i] = htole16(s);
	}
}


/*
 * wsdisplay_accessops: mmap()
 *   XXX: security considerations for allowing mmapping i/o mapped i/o regs?
 */
static paddr_t
igsfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd = v;
	struct igsfb_devconfig *dc = vd->cookie;

	if (offset < dc->dc_memsz && offset >= 0)
		return bus_space_mmap(dc->dc_memt, dc->dc_memaddr, offset,
		    prot, dc->dc_memflags | BUS_SPACE_MAP_LINEAR);
	if (dc->dc_mmap)
		return dc->dc_mmap(v, vs, offset, prot);
	return -1;
}


/*
 * wsdisplay_accessops: ioctl()
 */
static int
igsfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
	struct lwp *l)
{
	struct vcons_data *vd = v;
	struct igsfb_devconfig *dc = vd->cookie;
	int cursor_busy;
	int turnoff;

	/* don't permit cursor ioctls if we use sprite for text cursor */
	cursor_busy = !dc->dc_mapped
		&& (dc->dc_hwflags & IGSFB_HW_TEXT_CURSOR);

	switch (cmd) {

	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = dc->dc_height;
		wsd_fbip->width = dc->dc_width;
		wsd_fbip->depth = dc->dc_maxdepth;
		wsd_fbip->cmsize = IGS_CMAP_SIZE;
#undef wsd_fbip
		return 0;

	case WSDISPLAYIO_LINEBYTES:
		*(int *)data = dc->dc_width * (dc->dc_maxdepth >> 3);
		return 0;

	case WSDISPLAYIO_SMODE:
#define d (*(int *)data)
		if (d != WSDISPLAYIO_MODE_EMUL) {
			dc->dc_mapped = 1;
			/* turn off hardware cursor */
			if (dc->dc_hwflags & IGSFB_HW_TEXT_CURSOR) {
				dc->dc_curenb = 0;
				igsfb_update_cursor(dc,
					WSDISPLAY_CURSOR_DOCUR);
			}
			if ((dc->dc_mode != NULL) && (dc->dc_maxdepth != 8))
				igsfb_set_mode(dc, dc->dc_mode,
				    dc->dc_maxdepth);
		} else {
			dc->dc_mapped = 0;
			if ((dc->dc_mode != NULL) && (dc->dc_maxdepth != 8))
				igsfb_set_mode(dc, dc->dc_mode, 8);
			igsfb_init_cmap(dc);
			/* reinit sprite for text cursor */
			if (dc->dc_hwflags & IGSFB_HW_TEXT_CURSOR) {
				igsfb_make_text_cursor(dc, dc->dc_vd.active);
				dc->dc_curenb = 0;
				igsfb_update_cursor(dc,
					  WSDISPLAY_CURSOR_DOSHAPE
					| WSDISPLAY_CURSOR_DOCUR);
			}
			vcons_redraw_screen(vd->active);
		}
		return 0;

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = dc->dc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return 0;

	case WSDISPLAYIO_SVIDEO:
		turnoff = (*(u_int *)data == WSDISPLAYIO_VIDEO_OFF);
		if (dc->dc_blanked != turnoff) {
			dc->dc_blanked = turnoff;
			igsfb_blank_screen(dc, dc->dc_blanked);
		}
		return 0;

	case WSDISPLAYIO_GETCMAP:
		return igsfb_get_cmap(dc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return igsfb_set_cmap(dc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x = IGS_CURSOR_MAX_SIZE;
		((struct wsdisplay_curpos *)data)->y = IGS_CURSOR_MAX_SIZE;
		return 0;

	case WSDISPLAYIO_GCURPOS:
		if (cursor_busy)
			return EBUSY;
		*(struct wsdisplay_curpos *)data = dc->dc_cursor.cc_pos;
		return 0;

	case WSDISPLAYIO_SCURPOS:
		if (cursor_busy)
			return EBUSY;
		igsfb_set_curpos(dc, (struct wsdisplay_curpos *)data);
		return 0;

	case WSDISPLAYIO_GCURSOR:
		if (cursor_busy)
			return EBUSY;
		return igsfb_get_cursor(dc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		if (cursor_busy)
			return EBUSY;
		return igsfb_set_cursor(dc, (struct wsdisplay_cursor *)data);
	}

	return EPASSTHROUGH;
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_SVIDEO)
 */
static void
igsfb_blank_screen(struct igsfb_devconfig *dc, int blank)
{

	igs_ext_write(dc->dc_iot, dc->dc_ioh,
		      IGS_EXT_SYNC_CTL,
		      blank ? IGS_EXT_SYNC_H0 | IGS_EXT_SYNC_V0
			    : 0);
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_GETCMAP)
 *   Served from the software cmap copy.
 */
static int
igsfb_get_cmap(struct igsfb_devconfig *dc, struct wsdisplay_cmap *p)
{
	u_int index, count;
	int err;

	index = p->index;
	count = p->count;

	if (index >= IGS_CMAP_SIZE || count > IGS_CMAP_SIZE - index)
		return EINVAL;

	err = copyout(&dc->dc_cmap.r[index], p->red, count);
	if (err)
		return err;
	err = copyout(&dc->dc_cmap.g[index], p->green, count);
	if (err)
		return err;
	err = copyout(&dc->dc_cmap.b[index], p->blue, count);
	if (err)
		return err;

	return 0;
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_PUTCMAP)
 *   Set the software cmap copy and propagate changed range to the device.
 */
static int
igsfb_set_cmap(struct igsfb_devconfig *dc, const struct wsdisplay_cmap *p)
{
	u_int index, count;
	uint8_t r[IGS_CMAP_SIZE];
	uint8_t g[IGS_CMAP_SIZE];
	uint8_t b[IGS_CMAP_SIZE];
	int error;

	index = p->index;
	count = p->count;
	if (index >= IGS_CMAP_SIZE || count > IGS_CMAP_SIZE - index)
		return EINVAL;
	error = copyin(p->red, &r[index], count);
	if (error)
		return error;
	error = copyin(p->green, &g[index], count);
	if (error)
		return error;
	error = copyin(p->blue, &b[index], count);
	if (error)
		return error;

	memcpy(&dc->dc_cmap.r[index], &r[index], count);
	memcpy(&dc->dc_cmap.g[index], &g[index], count);
	memcpy(&dc->dc_cmap.b[index], &b[index], count);

	/* propagate changes to the device */
	igsfb_update_cmap(dc, index, count);
	return 0;
}


/*
 * Propagate specified part of the software cmap copy to the device.
 */
static void
igsfb_update_cmap(struct igsfb_devconfig *dc, u_int index, u_int count)
{
	bus_space_tag_t t;
	bus_space_handle_t h;
	u_int last, i;

	if (index >= IGS_CMAP_SIZE)
		return;

	last = index + count;
	if (last > IGS_CMAP_SIZE)
		last = IGS_CMAP_SIZE;

	t = dc->dc_iot;
	h = dc->dc_ioh;

	/* start palette writing, index is autoincremented by hardware */
	bus_space_write_1(t, h, IGS_DAC_PEL_WRITE_IDX, index);

	for (i = index; i < last; ++i) {
		bus_space_write_1(t, h, IGS_DAC_PEL_DATA, dc->dc_cmap.r[i]);
		bus_space_write_1(t, h, IGS_DAC_PEL_DATA, dc->dc_cmap.g[i]);
		bus_space_write_1(t, h, IGS_DAC_PEL_DATA, dc->dc_cmap.b[i]);
	}
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_SCURPOS)
 */
static void
igsfb_set_curpos(struct igsfb_devconfig *dc,
		 const struct wsdisplay_curpos *curpos)
{
	struct rasops_info *ri = &dc->dc_vd.active->scr_ri;
	u_int x = curpos->x, y = curpos->y;

	if (x >= ri->ri_width)
		x = ri->ri_width - 1;
	if (y >= ri->ri_height)
		y = ri->ri_height - 1;

	dc->dc_cursor.cc_pos.x = x;
	dc->dc_cursor.cc_pos.y = y;

	/* propagate changes to the device */
	igsfb_update_curpos(dc);
}


static void
igsfb_update_curpos(struct igsfb_devconfig *dc)
{
	bus_space_tag_t t;
	bus_space_handle_t h;
	int x, xoff, y, yoff;

	xoff = 0;
	x = dc->dc_cursor.cc_pos.x - dc->dc_cursor.cc_hot.x;
	if (x < 0) {
		xoff = -x;
		x = 0;
	}

	yoff = 0;
	y = dc->dc_cursor.cc_pos.y - dc->dc_cursor.cc_hot.y;
	if (y < 0) {
		yoff = -y;
		y = 0;
	}

	t = dc->dc_iot;
	h = dc->dc_ioh;

	igs_ext_write(t, h, IGS_EXT_SPRITE_HSTART_LO, x & 0xff);
	igs_ext_write(t, h, IGS_EXT_SPRITE_HSTART_HI, (x >> 8) & 0x07);
	igs_ext_write(t, h, IGS_EXT_SPRITE_HPRESET, xoff & 0x3f);

	igs_ext_write(t, h, IGS_EXT_SPRITE_VSTART_LO, y & 0xff);
	igs_ext_write(t, h, IGS_EXT_SPRITE_VSTART_HI, (y >> 8) & 0x07);
	igs_ext_write(t, h, IGS_EXT_SPRITE_VPRESET, yoff & 0x3f);
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_GCURSOR)
 */
static int
igsfb_get_cursor(struct igsfb_devconfig *dc, struct wsdisplay_cursor *p)
{

	/* XXX: TODO */
	return 0;
}


/*
 * wsdisplay_accessops: ioctl(WSDISPLAYIO_SCURSOR)
 */
static int
igsfb_set_cursor(struct igsfb_devconfig *dc, const struct wsdisplay_cursor *p)
{
	struct igs_hwcursor *cc;
	struct wsdisplay_curpos pos, hot;
	u_int v, index, count, icount, iwidth;
	uint8_t r[2], g[2], b[2], image[512], mask[512];
	int error;

	cc = &dc->dc_cursor;
	v = p->which;
	index = count = icount = iwidth = 0;	/* XXX: gcc */
	pos.x = pos.y = 0;			/* XXX: gcc */

	/* copy in the new cursor colormap */
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		index = p->cmap.index;
		count = p->cmap.count;
		if (index >= 2 || (index + count) > 2)
			return EINVAL;
		error = copyin(p->cmap.red, &r[index], count);
		if (error)
			return error;
		error = copyin(p->cmap.green, &g[index], count);
		if (error)
			return error;
		error = copyin(p->cmap.blue, &b[index], count);
		if (error)
			return error;
	}

	/* verify that the new cursor data are valid */
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if (p->size.x > IGS_CURSOR_MAX_SIZE
		    || p->size.y > IGS_CURSOR_MAX_SIZE)
			return EINVAL;

		iwidth = (p->size.x + 7) >> 3; /* bytes per scan line */
		icount = iwidth * p->size.y;
		error = copyin(p->image, image, icount);
		if (error)
			return error;
		error = copyin(p->mask, mask, icount);
		if (error)
			return error;
	}

	/* enforce that the position is within screen bounds */
	if (v & WSDISPLAY_CURSOR_DOPOS) {
		struct rasops_info *ri = &dc->dc_vd.active->scr_ri;

		pos = p->pos;	/* local copy we can write to */
		if (pos.x >= ri->ri_width)
			pos.x = ri->ri_width - 1;
		if (pos.y >= ri->ri_height)
			pos.y = ri->ri_height - 1;
	}

	/* enforce that the hot spot is within sprite bounds */
	if (v & WSDISPLAY_CURSOR_DOHOT)
		hot = p->hot;	/* local copy we can write to */

	if (v & (WSDISPLAY_CURSOR_DOHOT | WSDISPLAY_CURSOR_DOSHAPE)) {
		const struct wsdisplay_curpos *nsize;
		struct wsdisplay_curpos *nhot;

		nsize = (v & WSDISPLAY_CURSOR_DOSHAPE) ?
			    &p->size : &cc->cc_size;
		nhot = (v & WSDISPLAY_CURSOR_DOHOT) ? &hot : &cc->cc_hot;

		if (nhot->x >= nsize->x)
			nhot->x = nsize->x - 1;
		if (nhot->y >= nsize->y)
			nhot->y = nsize->y - 1;
	}

	/* copy data to the driver's cursor info */
	if (v & WSDISPLAY_CURSOR_DOCUR)
		dc->dc_curenb = p->enable;
	if (v & WSDISPLAY_CURSOR_DOPOS)
		cc->cc_pos = pos; /* local copy, possibly corrected */
	if (v & WSDISPLAY_CURSOR_DOHOT)
		cc->cc_hot = hot; /* local copy, possibly corrected */
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		memcpy(&cc->cc_color[index], &r[index], count);
		memcpy(&cc->cc_color[index + 2], &g[index], count);
		memcpy(&cc->cc_color[index + 4], &b[index], count);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		u_int trailing_bits;

		memcpy(cc->cc_image, image, icount);
		memcpy(cc->cc_mask, mask, icount);
		cc->cc_size = p->size;

		/* clear trailing bits in the "partial" mask bytes */
		trailing_bits = p->size.x & 0x07;
		if (trailing_bits != 0) {
			const u_int cutmask = ~((~0) << trailing_bits);
			u_char *mp;
			u_int i;

			mp = cc->cc_mask + iwidth - 1;
			for (i = 0; i < p->size.y; ++i) {
				*mp &= cutmask;
				mp += iwidth;
			}
		}
		igsfb_convert_cursor_data(dc, iwidth, p->size.y);
	}

	/* propagate changes to the device */
	igsfb_update_cursor(dc, v);

	return 0;
}


/*
 * Convert incoming 1bpp cursor image/mask into native 2bpp format.
 */
static void
igsfb_convert_cursor_data(struct igsfb_devconfig *dc,
			  u_int width, u_int height)
{
	struct igs_hwcursor *cc = &dc->dc_cursor;
	uint16_t *expand = dc->dc_bexpand;
	uint8_t *ip, *mp;
	uint16_t is, ms, *dp;
	u_int line, i;

	/* init sprite to be all transparent */
	memset(cc->cc_sprite, 0xaa, IGS_CURSOR_DATA_SIZE);

	/* first scanline */
	ip = cc->cc_image;
	mp = cc->cc_mask;
	dp = cc->cc_sprite;

	for (line = 0; line < height; ++line) {
		for (i = 0; i < width; ++i) {
			is = expand[ip[i]]; /* image: 0 -> 00, 1 -> 01 */
			ms = expand[mp[i]]; /*  mask: 0 -> 00, 1 -> 11 */
			ms |= (ms << 1);
			dp[i] = (0xaaaa & ~ms) | (is & ms);
		}

		/* next scanline */
		ip += width;
		mp += width;
		dp += 8; /* 64 pixels, 2bpp, 8 pixels per short = 8 shorts */
	}
}


/*
 * Propagate cursor changes to the device.
 * "which" is composed of WSDISPLAY_CURSOR_DO* bits.
 */
static void
igsfb_update_cursor(struct igsfb_devconfig *dc, u_int which)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	uint8_t curctl;

	curctl = 0;		/* XXX: gcc */

	/*
	 * We will need to tweak sprite control register for cursor
	 * visibility and cursor color map manipulation.
	 */
	if (which & (WSDISPLAY_CURSOR_DOCUR | WSDISPLAY_CURSOR_DOCMAP)) {
		curctl = igs_ext_read(iot, ioh, IGS_EXT_SPRITE_CTL);
	}

	if (which & WSDISPLAY_CURSOR_DOSHAPE) {
		uint8_t *dst = bus_space_vaddr(dc->dc_memt, dc->dc_crh);

		/*
		 * memcpy between spaces of different endianness would
		 * be ... special.  We cannot be sure if memcpy gonna
		 * push data in 4-byte chunks, we can't pre-bswap it,
		 * so do it byte-by-byte to preserve byte ordering.
		 */
		if (IGSFB_HW_SOFT_BSWAP(dc)) {
			uint8_t *src = (uint8_t *)dc->dc_cursor.cc_sprite;
			int i;

			for (i = 0; i < IGS_CURSOR_DATA_SIZE; ++i)
				*dst++ = *src++;
		} else {
			memcpy(dst, dc->dc_cursor.cc_sprite,
			       IGS_CURSOR_DATA_SIZE);
		}
	}

	if (which & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		/* code shared with WSDISPLAYIO_SCURPOS */
		igsfb_update_curpos(dc);
	}

	if (which & WSDISPLAY_CURSOR_DOCMAP) {
		uint8_t *p;

		/* tell DAC we want access to the cursor palette */
		igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL,
			      curctl | IGS_EXT_SPRITE_DAC_PEL);

		p = dc->dc_cursor.cc_color;

		bus_space_write_1(iot, ioh, IGS_DAC_PEL_WRITE_IDX, 0);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[0]);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[2]);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[4]);

		bus_space_write_1(iot, ioh, IGS_DAC_PEL_WRITE_IDX, 1);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[1]);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[3]);
		bus_space_write_1(iot, ioh, IGS_DAC_PEL_DATA, p[5]);

		/* restore access to the normal palette */
		igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL, curctl);
	}

	if (which & WSDISPLAY_CURSOR_DOCUR) {
		if ((curctl & IGS_EXT_SPRITE_VISIBLE) == 0
		    && dc->dc_curenb)
			igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL,
				      curctl | IGS_EXT_SPRITE_VISIBLE);
		else if ((curctl & IGS_EXT_SPRITE_VISIBLE) != 0
			 && !dc->dc_curenb)
			igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL,
				      curctl & ~IGS_EXT_SPRITE_VISIBLE);
	}
}


/*
 * Accelerated text mode cursor that uses hardware sprite.
 */
static void
igsfb_accel_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = scr->scr_cookie;
	struct igs_hwcursor *cc = &dc->dc_cursor;
	u_int which;

	ri->ri_crow = row;
	ri->ri_ccol = col;

	which = 0;
	if (on) {
		ri->ri_flg |= RI_CURSOR;

		/* only bother to move the cursor if it's visible */
		cc->cc_pos.x = ri->ri_xorigin
			+ ri->ri_ccol * ri->ri_font->fontwidth;
		cc->cc_pos.y = ri->ri_yorigin
			+ ri->ri_crow * ri->ri_font->fontheight;
		which |= WSDISPLAY_CURSOR_DOPOS;
	} else
		ri->ri_flg &= ~RI_CURSOR;

	if (dc->dc_curenb != on) {
		dc->dc_curenb = on;
		which |= WSDISPLAY_CURSOR_DOCUR;
	}

	/* propagate changes to the device */
	igsfb_update_cursor(dc, which);
}



/*
 * Accelerated raster ops that use graphic coprocessor.
 */

static int
igsfb_accel_wait(struct igsfb_devconfig *dc)
{
	bus_space_tag_t t = dc->dc_iot;
	bus_space_handle_t h = dc->dc_coph;
	int timo = 100000;
	uint8_t reg;

	bus_space_write_1(t, h, IGS_COP_MAP_FMT_REG, (dc->dc_depth >> 3) - 1);
	while (timo--) {
		reg = bus_space_read_1(t, h, IGS_COP_CTL_REG);
		if ((reg & IGS_COP_CTL_BUSY) == 0)
			return 0;
	}

	return 1;
}


static void
igsfb_accel_copy(struct igsfb_devconfig *dc, uint32_t src, uint32_t dst,
		 uint16_t width, uint16_t height)
{
	bus_space_tag_t t = dc->dc_iot;
	bus_space_handle_t h = dc->dc_coph;
	uint32_t toend;
	uint8_t drawcmd;

	drawcmd = IGS_COP_DRAW_ALL;
	if (dst > src) {
		toend = dc->dc_vd.active->scr_ri.ri_width * (height - 1) + (width - 1);
		src += toend;
		dst += toend;
		drawcmd |= IGS_COP_OCTANT_X_NEG | IGS_COP_OCTANT_Y_NEG;
	}

	igsfb_accel_wait(dc);
	bus_space_write_1(t, h, IGS_COP_CTL_REG, 0);

	bus_space_write_1(t, h, IGS_COP_FG_MIX_REG, IGS_COP_MIX_S);

	bus_space_write_2(t, h, IGS_COP_WIDTH_REG, width - 1);
	bus_space_write_2(t, h, IGS_COP_HEIGHT_REG, height - 1);

	bus_space_write_4(t, h, IGS_COP_SRC_START_REG, src);
	bus_space_write_4(t, h, IGS_COP_DST_START_REG, dst);

	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_0_REG, drawcmd);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_1_REG, IGS_COP_PPM_FIXED_FG);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_2_REG, 0);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_3_REG,
			  IGS_COP_OP_PXBLT | IGS_COP_OP_FG_FROM_SRC);
}


static void
igsfb_accel_fill(struct igsfb_devconfig *dc, uint32_t color, uint32_t dst,
		 uint16_t width, uint16_t height)
{
	bus_space_tag_t t = dc->dc_iot;
	bus_space_handle_t h = dc->dc_coph;

	igsfb_accel_wait(dc);
	bus_space_write_1(t, h, IGS_COP_CTL_REG, 0);

	bus_space_write_1(t, h, IGS_COP_FG_MIX_REG, IGS_COP_MIX_S);

	bus_space_write_2(t, h, IGS_COP_WIDTH_REG, width - 1);
	bus_space_write_2(t, h, IGS_COP_HEIGHT_REG, height - 1);

	bus_space_write_4(t, h, IGS_COP_DST_START_REG, dst);
	bus_space_write_4(t, h, IGS_COP_FG_REG, color);

	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_0_REG, IGS_COP_DRAW_ALL);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_1_REG, IGS_COP_PPM_FIXED_FG);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_2_REG, 0);
	bus_space_write_1(t, h, IGS_COP_PIXEL_OP_3_REG, IGS_COP_OP_PXBLT);
}


static void
igsfb_accel_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = (struct igsfb_devconfig *)scr->scr_cookie;
	uint32_t srp, dsp;
	uint16_t width, height;

	width = ri->ri_emuwidth;
	height = num * ri->ri_font->fontheight;

	srp = ri->ri_xorigin
		+ ri->ri_width * (ri->ri_yorigin
				  + src * ri->ri_font->fontheight);
	dsp = ri->ri_xorigin
		+ ri->ri_width * (ri->ri_yorigin
				  + dst * ri->ri_font->fontheight);

	igsfb_accel_copy(dc, srp, dsp, width, height);
}


static void
igsfb_accel_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = (struct igsfb_devconfig *)scr->scr_cookie;
	uint32_t rowp, srp, dsp;
	uint16_t width, height;

	width = num * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	rowp = ri->ri_xorigin
		+ ri->ri_width * (ri->ri_yorigin
				  + row * ri->ri_font->fontheight);

	srp = rowp + src * ri->ri_font->fontwidth;
	dsp = rowp + dst * ri->ri_font->fontwidth;

	igsfb_accel_copy(dc, srp, dsp, width, height);
}


static void
igsfb_accel_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = (struct igsfb_devconfig *)scr->scr_cookie;
	uint32_t color;
	uint32_t dsp;
	uint16_t width, height;

	if (num == ri->ri_rows && (ri->ri_flg & RI_FULLCLEAR) != 0) {
		width = ri->ri_width;
		height = ri->ri_height;
		dsp = 0;
	} else {
		width = ri->ri_emuwidth;
		height = num * ri->ri_font->fontheight;

		dsp = ri->ri_xorigin
			+ ri->ri_width * (ri->ri_yorigin
					  + row * ri->ri_font->fontheight);
	}

	/* XXX: we "know" the encoding that rasops' allocattr uses */
	color = (attr >> 16) & 0xff;

	igsfb_accel_fill(dc, color, dsp, width, height);
}


static void
igsfb_accel_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = (struct igsfb_devconfig *)scr->scr_cookie;
	uint32_t color;
	uint32_t rowp, dsp;
	uint16_t width, height;

	width = num * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;

	rowp = ri->ri_xorigin
		+ ri->ri_width * (ri->ri_yorigin
				  + row * ri->ri_font->fontheight);

	dsp = rowp + col * ri->ri_font->fontwidth;

	/* XXX: we "know" the encoding that rasops' allocattr uses */
	color = (attr >> 16) & 0xff;

	igsfb_accel_fill(dc, color, dsp, width, height);
}


/*
 * Not really implemented here, but we need to hook into the one
 * supplied by rasops so that we can synchronize with the COP.
 */
static void
igsfb_accel_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = (struct rasops_info *)cookie;
	struct vcons_screen *scr = ri->ri_hw;
	struct igsfb_devconfig *dc = (struct igsfb_devconfig *)scr->scr_cookie;

	igsfb_accel_wait(dc);
	(*dc->dc_ri_putchar)(cookie, row, col, uc, attr);
}
