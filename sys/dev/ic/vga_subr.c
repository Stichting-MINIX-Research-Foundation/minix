/* $NetBSD: vga_subr.c,v 1.25 2012/08/09 20:25:05 uwe Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vga_subr.c,v 1.25 2012/08/09 20:25:05 uwe Exp $");

/* for WSDISPLAY_BORDER_COLOR */
#include "opt_wsdisplay_border.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/bus.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplay.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>

#include <dev/wscons/wsdisplayvar.h>

static void fontram(struct vga_handle *);
static void textram(struct vga_handle *);

static void
fontram(struct vga_handle *vh)
{

	/* program sequencer to access character generator */

	vga_ts_write(vh, syncreset, 0x01);	/* synchronous reset */
	vga_ts_write(vh, wrplmask, 0x04);	/* write to map 2 */
	vga_ts_write(vh, memmode, 0x07);	/* sequential addressing */
	vga_ts_write(vh, syncreset, 0x03);	/* clear synchronous reset */

	/* program graphics controller to access character generator */

	vga_gdc_write(vh, rdplanesel, 0x02);	/* select map 2 for CPU reads */
	vga_gdc_write(vh, mode, 0x00);	/* disable odd-even addressing */
	vga_gdc_write(vh, misc, 0x04);	/* map starts at 0xA000 */
}

static void
textram(struct vga_handle *vh)
{

	/* program sequencer to access video ram */

	vga_ts_write(vh, syncreset, 0x01);	/* synchronous reset */
	vga_ts_write(vh, wrplmask, 0x03);	/* write to map 0 & 1 */
	vga_ts_write(vh, memmode, 0x03);	/* odd-even addressing */
	vga_ts_write(vh, syncreset, 0x03);	/* clear synchronous reset */

	/* program graphics controller for text mode */

	vga_gdc_write(vh, rdplanesel, 0x00);	/* select map 0 for CPU reads */
	vga_gdc_write(vh, mode, 0x10);		/* enable odd-even addressing */
	/* map starts at 0xb800 or 0xb000 (mono) */
	vga_gdc_write(vh, misc, (vh->vh_mono ? 0x0a : 0x0e));
}

#ifndef VGA_RASTERCONSOLE
void
vga_loadchars(struct vga_handle *vh, int fontset, int first, int num, int lpc,
	      const char *data)
{
	int offset, i, j, s;

	/* fontset number swizzle done in vga_setfontset() */
	offset = (fontset << 13) | (first << 5);

	s = splhigh();
	fontram(vh);

	for (i = 0; i < num; i++)
		for (j = 0; j < lpc; j++)
			bus_space_write_1(vh->vh_memt, vh->vh_allmemh,
			    offset + (i << 5) + j, data[i * lpc + j]);

	textram(vh);
	splx(s);
}

void
vga_readoutchars(struct vga_handle *vh, int fontset, int first, int num,
		 int lpc, char *data)
{
	int offset, i, j, s;

	/* fontset number swizzle done in vga_setfontset() */
	offset = (fontset << 13) | (first << 5);

	s = splhigh();
	fontram(vh);

	for (i = 0; i < num; i++)
		for (j = 0; j < lpc; j++)
			data[i * lpc + j] = bus_space_read_1(vh->vh_memt,
			    vh->vh_allmemh, offset + (i << 5) + j);

	textram(vh);
	splx(s);
}

#ifdef VGA_CONSOLE_ATI_BROKEN_FONTSEL
void
vga_copyfont01(struct vga_handle *vh)
{
	int s;

	s = splhigh();
	fontram(vh);

	bus_space_copy_region_1(vh->vh_memt, vh->vh_allmemh, 0,
	    vh->vh_allmemh, 1 << 13, 1 << 13);

	textram(vh);
	splx(s);
}
#endif

void
vga_setfontset(struct vga_handle *vh, int fontset1, int fontset2)
{
	uint8_t cmap;
	static const uint8_t cmaptaba[] = {
		0x00, 0x10, 0x01, 0x11,
		0x02, 0x12, 0x03, 0x13
	};
	static const uint8_t cmaptabb[] = {
		0x00, 0x20, 0x04, 0x24,
		0x08, 0x28, 0x0c, 0x2c
	};

	/* extended font if fontset1 != fontset2 */
	cmap = cmaptaba[fontset1] | cmaptabb[fontset2];

	vga_ts_write(vh, fontsel, cmap);
}

void
vga_setscreentype(struct vga_handle *vh, const struct wsscreen_descr *type)
{

	vga_6845_write(vh, maxrow, type->fontheight - 1);

	/* lo byte */
	vga_6845_write(vh, vde, type->fontheight * type->nrows - 1);

#ifndef PCDISPLAY_SOFTCURSOR
	/* set cursor to last 2 lines */
	vga_6845_write(vh, curstart, type->fontheight - 2);
	vga_6845_write(vh, curend, type->fontheight - 1);
#endif
	/*
	 * disable colour plane 3 if needed for font selection
	 */
	if (type->capabilities & WSSCREEN_HILIT) {
		/*
		 * these are the screens which don't support
		 * 512-character fonts
		 */
		vga_attr_write(vh, colplen, 0x0f);
	} else
		vga_attr_write(vh, colplen, 0x07);
}

#else /* !VGA_RASTERCONSOLE */
void
vga_load_builtinfont(struct vga_handle *vh, uint8_t *font, int firstchar,
	int numchars)
{
	int i, s;

	s = splhigh();
	fontram(vh);

	for (i = firstchar; i < firstchar + numchars; i++)
		bus_space_read_region_1(vh->vh_memt, vh->vh_allmemh, i * 32,
		    font + i * 16, 16);

	textram(vh);
	splx(s);
}
#endif /* !VGA_RASTERCONSOLE */

/*
 * vga_reset():
 *	Reset VGA registers to put it into 80x25 text mode. (mode 3)
 *	This function should be called from MD consinit() on ports
 *	whose firmware does not use text mode at boot time.
 */
void
vga_reset(struct vga_handle *vh, void (*md_initfunc)(struct vga_handle *))
{
	uint8_t reg;

	if (bus_space_map(vh->vh_iot, 0x3c0, 0x10, 0, &vh->vh_ioh_vga))
		return;

	reg = vga_raw_read(vh, VGA_MISC_DATAR);
	vh->vh_mono = !(reg & 0x01);

	if (bus_space_map(vh->vh_iot, vh->vh_mono ? 0x3b0 : 0x3d0, 0x10,
	    0, &vh->vh_ioh_6845))
		goto out1;

	if (bus_space_map(vh->vh_memt, 0xa0000, 0x20000, 0, &vh->vh_allmemh))
		goto out2;

	if (bus_space_subregion(vh->vh_memt, vh->vh_allmemh,
	    vh->vh_mono ? 0x10000 : 0x18000, 0x8000, &vh->vh_memh))
		goto out3;

	/* check if VGA already in text mode. */
	if ((vga_gdc_read(vh, misc) & 0x01) == 0)
		goto out3;

	/* initialize common VGA registers */
	vga_initregs(vh);

	/* initialize chipset specific registers */
	if (md_initfunc != NULL)
		(*md_initfunc)(vh);

	delay(10000);

	/* clear text buffer RAM */
	bus_space_set_region_2(vh->vh_memt, vh->vh_memh, 0,
	    ((BG_BLACK | FG_LIGHTGREY) << 8) | ' ', 80 * 25 /*XXX*/);

 out3:
	bus_space_unmap(vh->vh_memt, vh->vh_allmemh, 0x20000);
 out2:
	bus_space_unmap(vh->vh_iot, vh->vh_ioh_6845, 0x10);
 out1:
	bus_space_unmap(vh->vh_iot, vh->vh_ioh_vga, 0x10);
}

/*
 * values to initialize registers.
 */

/* miscellaneous output register */
#define VGA_MISCOUT	0x66

/* sequencer registers */
static const uint8_t vga_ts[] = {
	0x03,	/* 00: reset */
	0x00,	/* 01: clocking mode */
	0x03,	/* 02: map mask */
	0x00,	/* 03: character map select */
	0x02	/* 04: memory mode */
};

/* CRT controller registers */
static const uint8_t vga_crtc[] = {
	0x5f,	/* 00: horizontal total */
	0x4f,	/* 01: horizontal display-enable end */
	0x50,	/* 02: start horizontal blanking */
	0x82,	/* 03: display skew control / end horizontal blanking */
	0x55,	/* 04: start horizontal retrace pulse */
	0x81,	/* 05: horizontal retrace delay / end horizontal retrace */
	0xbf,	/* 06: vertical total */
	0x1f,	/* 07: overflow register */
	0x00,	/* 08: preset row scan */
	0x4f,	/* 09: overflow / maximum scan line */
	0x0d,	/* 0A: cursor off / cursor start */
	0x0e,	/* 0B: cursor skew / cursor end */
	0x00,	/* 0C: start regenerative buffer address high */
	0x00,	/* 0D: start regenerative buffer address low */
	0x00,	/* 0E: cursor location high */
	0x00,	/* 0F: cursor location low */
	0x9c,	/* 10: vertical retrace start */
	0x8e,	/* 11: vertical interrupt / vertical retrace end */
	0x8f,	/* 12: vertical display enable end */
	0x28,	/* 13: logical line width */
	0x00,	/* 14: underline location */
	0x96,	/* 15: start vertical blanking */
	0xb9,	/* 16: end vertical blanking */
	0xa3,	/* 17: CRT mode control */
	0xff	/* 18: line compare */
};

/* graphics controller registers */
static const uint8_t vga_gdc[] = {
	0x00,	/* 00: set/reset map */
	0x00,	/* 01: enable set/reset */
	0x00,	/* 02: color compare */
	0x00,	/* 03: data rotate */
	0x00,	/* 04: read map select */
	0x10,	/* 05: graphics mode */
	0x0e,	/* 06: miscellaneous */
	0x00,	/* 07: color don't care */
	0xff	/* 08: bit mask */
};

/* attribute controller registers */
static const uint8_t vga_atc[] = {
	0x00,	/* 00: internal palette  0 */
	0x01,	/* 01: internal palette  1 */
	0x02,	/* 02: internal palette  2 */
	0x03,	/* 03: internal palette  3 */
	0x04,	/* 04: internal palette  4 */
	0x05,	/* 05: internal palette  5 */
	0x14,	/* 06: internal palette  6 */
	0x07,	/* 07: internal palette  7 */
	0x38,	/* 08: internal palette  8 */
	0x39,	/* 09: internal palette  9 */
	0x3a,	/* 0A: internal palette 10 */
	0x3b,	/* 0B: internal palette 11 */
	0x3c,	/* 0C: internal palette 12 */
	0x3d,	/* 0D: internal palette 13 */
	0x3e,	/* 0E: internal palette 14 */
	0x3f,	/* 0F: internal palette 15 */
	0x0c,	/* 10: attribute mode control */
	WSDISPLAY_BORDER_COLOR,	/* 11: overscan color */
	0x0f,	/* 12: color plane enable */
	0x08,	/* 13: horizontal PEL panning */
	0x00	/* 14: color select */
};

/* video DAC palette registers */
/* XXX only set up 16 colors used by internal palette in ATC regsters */
static const uint8_t vga_dacpal[] = {
	/* R     G     B */
	0x00, 0x00, 0x00,	/* BLACK        */
	0x00, 0x00, 0x2a,	/* BLUE	        */
	0x00, 0x2a, 0x00,	/* GREEN        */
	0x00, 0x2a, 0x2a,	/* CYAN         */
	0x2a, 0x00, 0x00,	/* RED          */
	0x2a, 0x00, 0x2a,	/* MAGENTA      */
	0x2a, 0x15, 0x00,	/* BROWN        */
	0x2a, 0x2a, 0x2a,	/* LIGHTGREY    */
	0x15, 0x15, 0x15,	/* DARKGREY     */
	0x15, 0x15, 0x3f,	/* LIGHTBLUE    */
	0x15, 0x3f, 0x15,	/* LIGHTGREEN   */
	0x15, 0x3f, 0x3f,	/* LIGHTCYAN    */
	0x3f, 0x15, 0x15,	/* LIGHTRED     */
	0x3f, 0x15, 0x3f,	/* LIGHTMAGENTA */
	0x3f, 0x3f, 0x15,	/* YELLOW       */
	0x3f, 0x3f, 0x3f	/* WHITE        */
};

void
vga_initregs(struct vga_handle *vh)
{
	int i;

	/* disable video */
	vga_ts_write(vh, mode, vga_ts[1] | VGA_TS_MODE_BLANK);

	/* synchronous reset */
	vga_ts_write(vh, syncreset, 0x01);
	/* set TS regsters */
	for (i = 2; i < VGA_TS_NREGS; i++)
		_vga_ts_write(vh, i, vga_ts[i]);
	/* clear synchronous reset */
	vga_ts_write(vh, syncreset, 0x03);

	/* unprotect CRTC regsters */
	vga_6845_write(vh, vsynce, vga_6845_read(vh, vsynce) & ~0x80);
	/* set CRTC regsters */
	for (i = 0; i < MC6845_NREGS; i++)
		_vga_6845_write(vh, i, vga_crtc[i]);

	/* set GDC regsters */
	for (i = 0; i < VGA_GDC_NREGS; i++)
		_vga_gdc_write(vh, i, vga_gdc[i]);

	/* set ATC regsters */
	for (i = 0; i < VGA_ATC_NREGS; i++)
		_vga_attr_write(vh, i, vga_atc[i]);

	/* set DAC palette */
	if (!vh->vh_mono) {
		for (i = 0; i < 16; i++) {
			vga_raw_write(vh,
			    VGA_DAC_ADDRW, vga_atc[i]);
			vga_raw_write(vh,
			    VGA_DAC_PALETTE, vga_dacpal[i * 3 + 0]);
			vga_raw_write(vh,
			    VGA_DAC_PALETTE, vga_dacpal[i * 3 + 1]);
			vga_raw_write(vh,
			    VGA_DAC_PALETTE, vga_dacpal[i * 3 + 2]);
		}
	}

	/* set misc output register */
	vga_raw_write(vh,
	    VGA_MISC_DATAW, VGA_MISCOUT | (vh->vh_mono ? 0 : 0x01));

	/* reenable video */
	vga_ts_write(vh, mode, vga_ts[1] & ~VGA_TS_MODE_BLANK);
}
