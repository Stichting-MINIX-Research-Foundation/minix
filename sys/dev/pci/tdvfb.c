/*	$NetBSD: tdvfb.c,v 1.8 2014/02/28 05:55:23 matt Exp $	*/

/*
 * Copyright (c) 2012 The NetBSD Foundation, Inc.   
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Radoslaw Kujawa.
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
 */

/*
 * A console driver for 3Dfx Voodoo2 (CVG) and 3Dfx Voodoo Graphics (SST-1).
 *
 * 3Dfx Glide 2.x source code, Linux driver by Ghozlane Toumi, and 
 * "Voodoo2 Graphics Engine for 3D Game Acceleration" document were used as 
 * reference. wscons attachment code based mostly on genfb by Michael
 * Lorenz.
 *
 * This driver currently only support boards with ICS GENDAC (which seems to
 * be most popular, however at least two different DACs were used with CVG).
 *
 * TODO (in no particular order):
 * - Finally fix 16-bit depth handling on big-endian machines.
 * - Expose card to userspace through /dev/3dfx compatible device file
 *   (for Glide).
 * - Allow mmap'ing of registers through wscons access op.
 * - Complete wscons emul ops acceleration support.
 * - Add support for others DACs (need hardware).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tdvfb.c,v 1.8 2014/02/28 05:55:23 matt Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/endian.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/pci/tdvfbreg.h>
#include <dev/pci/tdvfbvar.h>

#include <dev/videomode/videomode.h>
#include <dev/pci/wsdisplay_pci.h>

#include "opt_wsemul.h"
#include "opt_tdvfb.h"

#define MAXLOOP 4096 
/* #define TDVFB_DEBUG 1 */

static int	tdvfb_match(device_t, cfdata_t, void *);
static void	tdvfb_attach(device_t, device_t, void *);

static uint32_t	tdvfb_cvg_read(struct tdvfb_softc *sc, uint32_t reg);
static void	tdvfb_cvg_write(struct tdvfb_softc *sc, uint32_t reg,
		    uint32_t val);
static void	tdvfb_cvg_set(struct tdvfb_softc *sc, uint32_t reg, 
		    uint32_t bits);
static void	tdvfb_cvg_unset(struct tdvfb_softc *sc, uint32_t reg, 
		    uint32_t bits);
static uint8_t	tdvfb_cvg_dac_read(struct tdvfb_softc *sc, uint32_t reg);
static void	tdvfb_cvg_dac_write(struct tdvfb_softc *sc, uint32_t reg, 
		    uint32_t val);
static void	tdvfb_wait(struct tdvfb_softc *sc);

static bool	tdvfb_init(struct tdvfb_softc *sc);
static void	tdvfb_fbiinit_defaults(struct tdvfb_softc *sc);
static size_t	tdvfb_mem_size(struct tdvfb_softc *sc);

static bool	tdvfb_videomode_set(struct tdvfb_softc *sc);
static void	tdvfb_videomode_dac(struct tdvfb_softc *sc);

static bool	tdvfb_gendac_detect(struct tdvfb_softc *sc);
static struct tdvfb_dac_timing	tdvfb_gendac_calc_pll(int freq);
static void	tdvfb_gendac_set_cvg_timing(struct tdvfb_softc *sc, 
		    struct tdvfb_dac_timing *timing);
static void	tdvfb_gendac_set_vid_timing(struct tdvfb_softc *sc, 
		    struct tdvfb_dac_timing *timing);

static paddr_t	tdvfb_mmap(void *v, void *vs, off_t offset, int prot);
static int	tdvfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag,
		    struct lwp *l);
static void	tdvfb_init_screen(void *cookie, struct vcons_screen *scr, 
		    int existing, long *defattr);
static void	tdvfb_init_palette(struct tdvfb_softc *sc);
/* blitter support */
static void	tdvfb_rectfill(struct tdvfb_softc *sc, int x, int y, int wi, 
		    int he, uint32_t color);
static void	tdvfb_bitblt(struct tdvfb_softc *sc, int xs, int ys, int xd, 
		    int yd, int wi, int he);
/* accelerated raster ops */
static void	tdvfb_eraserows(void *cookie, int row, int nrows, 
		    long fillattr);
static void	tdvfb_copyrows(void *cookie, int srcrow, int dstrow, int nrows);

CFATTACH_DECL_NEW(tdvfb, sizeof(struct tdvfb_softc),
    tdvfb_match, tdvfb_attach, NULL, NULL);

struct wsdisplay_accessops tdvfb_accessops = {
	tdvfb_ioctl,
	tdvfb_mmap,
	NULL,	/* alloc_screen */
	NULL,	/* free_screen */
	NULL,	/* show_screen */
	NULL, 	/* load_font */
	NULL,	/* pollc */
	NULL	/* scroll */
};

static int
tdvfb_match(device_t parent, cfdata_t match, void *aux)
{
	const struct pci_attach_args *pa = (const struct pci_attach_args *)aux;

	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_3DFX) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_3DFX_VOODOO2))
		return 100;
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_3DFX) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_3DFX_VOODOO))
		return 100;

	return 0;
}

static void
tdvfb_attach(device_t parent, device_t self, void *aux)
{
	struct tdvfb_softc *sc = device_private(self);
	struct wsemuldisplaydev_attach_args ws_aa;
	struct rasops_info *ri;
	const struct pci_attach_args *pa = aux;
	pcireg_t screg;
	bool console;
	long defattr;

#ifdef TDVFB_CONSOLE
	console = true; 
#else
	console = false;
#endif

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dev = self;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_3DFX_VOODOO2)
		sc->sc_voodootype = TDV_VOODOO_2;
	else
		sc->sc_voodootype = TDV_VOODOO_1;

	screg = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    PCI_COMMAND_STATUS_REG);
	screg |= PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, 
	    screg);

	pci_aprint_devinfo(pa, NULL);

	/* map the BAR */
	if (pci_mapreg_map(pa, TDV_MM_BAR, PCI_MAPREG_TYPE_MEM, 
	    BUS_SPACE_MAP_LINEAR, &sc->sc_cvgt, &sc->sc_cvgh, 
	    &sc->sc_cvg_pa, 0) != 0 ) {
		aprint_error_dev(sc->sc_dev, "unable to map CVG BAR");
		return;
	}

	/* Map the framebuffer. */
	if (bus_space_subregion(sc->sc_cvgt, sc->sc_cvgh, TDV_OFF_FB, 
	    TDV_FB_SIZE, &sc->sc_fbh)) {
		aprint_error_dev(sc->sc_dev, "unable to map the framebuffer");	
	}

	aprint_normal_dev(sc->sc_dev, "registers at 0x%08x, fb at 0x%08x\n", 
	    (uint32_t) sc->sc_cvg_pa, (uint32_t) sc->sc_cvg_pa + TDV_OFF_FB);

	/* Do the low level setup. */
	if (!tdvfb_init(sc)) {
		aprint_error_dev(sc->sc_dev, "could not initialize CVG\n");
		return;
	}

	/* 
	 * The card is alive now, let's check how much framebuffer memory 
	 * do we have.
	 */
	sc->sc_memsize = tdvfb_mem_size(sc);

	aprint_normal_dev(sc->sc_dev, "%zu MB framebuffer memory present\n", 
	    sc->sc_memsize / 1024 / 1024);

	/* Select video mode, 800x600 32bpp 60Hz by default... */
	sc->sc_width = 800;
	sc->sc_height = 600;
#if BYTE_ORDER == BIG_ENDIAN
	sc->sc_bpp = 32;	/* XXX: 16 would allow blitter use. */
#else
	sc->sc_bpp = 16;
#endif 
	sc->sc_linebytes = 1024 * (sc->sc_bpp / 8);
	sc->sc_videomode = pick_mode_by_ref(sc->sc_width, sc->sc_height, 60);

	aprint_normal_dev(sc->sc_dev, "setting %dx%d %d bpp resolution\n",
	    sc->sc_width, sc->sc_height, sc->sc_bpp);

	tdvfb_videomode_set(sc);

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
	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;
	
	vcons_init(&sc->vd, sc, &sc->sc_defaultscreen_descr,
	    &tdvfb_accessops);
	sc->vd.init_screen = tdvfb_init_screen;

	ri = &sc->sc_console_screen.scr_ri;

	tdvfb_init_palette(sc);

	if (console) {
		vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
		    &defattr);

		sc->sc_console_screen.scr_flags |= VCONS_SCREEN_IS_STATIC |
		    VCONS_DONT_READ; 
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
			vcons_init_screen(&sc->vd, &sc->sc_console_screen, 1,
			    &defattr);
		} else
			(*ri->ri_ops.allocattr)(ri, 0, 0, 0, &defattr);
	}

	ws_aa.console = console;
	ws_aa.scrdata = &sc->sc_screenlist;
	ws_aa.accessops = &tdvfb_accessops;
	ws_aa.accesscookie = &sc->vd;
	
	config_found(sc->sc_dev, &ws_aa, wsemuldisplaydevprint);
}

static void
tdvfb_init_palette(struct tdvfb_softc *sc)
{
	int i, j;

	j = 0;
	for (i = 0; i < 256; i++) {
		sc->sc_cmap_red[i] = rasops_cmap[j];
		sc->sc_cmap_green[i] = rasops_cmap[j + 1];
		sc->sc_cmap_blue[i] = rasops_cmap[j + 2];
		j += 3;
	}
}

static void
tdvfb_init_screen(void *cookie, struct vcons_screen *scr, int existing,
    long *defattr)
{
	struct tdvfb_softc *sc = cookie;
	struct rasops_info *ri = &scr->scr_ri;

	wsfont_init();

	ri->ri_depth = sc->sc_bpp;
	ri->ri_width = sc->sc_width;
	ri->ri_height = sc->sc_height;
	ri->ri_stride = sc->sc_linebytes;
	ri->ri_flg = RI_CENTER;

#if BYTE_ORDER == BIG_ENDIAN
#if 0 /* XXX: not yet :( */
	if (sc->sc_bpp == 16)
		ri->ri_flg |= RI_BITSWAP;
#endif 
#endif

	ri->ri_bits = (char *) bus_space_vaddr(sc->sc_cvgt, sc->sc_fbh);
#ifdef TDVFB_DEBUG
	aprint_normal_dev(sc->sc_dev, "fb handle: %lx, ri_bits: %p\n", sc->sc_fbh, ri->ri_bits);
#endif /* TDVFB_DEBUG */

	scr->scr_flags |= VCONS_DONT_READ;

	rasops_init(ri, 0, 0);
	ri->ri_caps = WSSCREEN_WSCOLORS;
	rasops_reconfig(ri, sc->sc_height / ri->ri_font->fontheight,
	    sc->sc_width / ri->ri_font->fontwidth);

	ri->ri_hw = scr;

	/* If we are a Voodoo2 and running in 16 bits try to use blitter. */
	if ((sc->sc_voodootype == TDV_VOODOO_2) && (sc->sc_bpp == 16)) {
		aprint_normal_dev(sc->sc_dev, "using CVG blitter\n");
		ri->ri_ops.eraserows = tdvfb_eraserows;
		ri->ri_ops.copyrows = tdvfb_copyrows;
	}
}

static bool
tdvfb_videomode_set(struct tdvfb_softc *sc) 
{
	uint32_t fbiinit1, fbiinit5, fbiinit6, lfbmode;
	uint16_t vbackporch, vsyncon, vsyncoff;
	uint16_t hbackporch, hsyncon, hsyncoff; 
	uint16_t yheight, xwidth; 

	fbiinit5 = fbiinit6 = 0; /* XXX gcc */

	yheight = sc->sc_videomode->vdisplay;
	xwidth = sc->sc_videomode->hdisplay;

	vbackporch = sc->sc_videomode->vtotal - sc->sc_videomode->vsync_end;
	hbackporch = sc->sc_videomode->htotal - sc->sc_videomode->hsync_end;

	vsyncon = sc->sc_videomode->vsync_end - sc->sc_videomode->vsync_start;
	hsyncon = sc->sc_videomode->hsync_end - sc->sc_videomode->hsync_start;

	vsyncoff = sc->sc_videomode->vtotal - vsyncon;
	hsyncoff = sc->sc_videomode->htotal - hsyncon;
#ifdef TDVFB_DEBUG 
	aprint_normal_dev(sc->sc_dev, 
	    "xy %d %d hbp %d vbp %d, hson %d, hsoff %d, vson %d, vsoff %d\n",
	    xwidth, yheight, hbackporch, vbackporch, hsyncon, hsyncoff, 
	    vsyncon, vsyncoff);
#endif /* TDVFB_DEBUG */

	sc->vid_timing = tdvfb_gendac_calc_pll(sc->sc_videomode->dot_clock);

	if(sc->sc_voodootype == TDV_VOODOO_2)
		sc->sc_x_tiles = (sc->sc_videomode->hdisplay + 63 ) / 64 * 2;
	else
		sc->sc_x_tiles = (sc->sc_videomode->hdisplay + 63 ) / 64;

	tdvfb_cvg_write(sc, TDV_OFF_NOPCMD, 0);
	tdvfb_wait(sc);

	/* enable writing to fbiinit regs, reset, disable DRAM refresh */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT);
	tdvfb_cvg_set(sc, TDV_OFF_FBIINIT1, TDV_FBIINIT1_VIDEO_RST);
	tdvfb_cvg_set(sc, TDV_OFF_FBIINIT0, TDV_FBIINIT0_FBI_RST | 
	    TDV_FBIINIT0_FIFO_RST);
	tdvfb_cvg_unset(sc, TDV_OFF_FBIINIT2, TDV_FBIINIT2_DRAM_REFR);
	tdvfb_wait(sc);

	/* program video timings into CVG/SST-1*/
	tdvfb_cvg_write(sc, TDV_OFF_VDIMENSIONS, yheight << 16 | (xwidth - 1));
	tdvfb_cvg_write(sc, TDV_OFF_BACKPORCH, vbackporch << 16 | 
	    (hbackporch - 2));
	tdvfb_cvg_write(sc, TDV_OFF_HSYNC, hsyncoff << 16 | (hsyncon - 1));
	tdvfb_cvg_write(sc, TDV_OFF_VSYNC, vsyncoff << 16 | vsyncon);

	tdvfb_videomode_dac(sc);

	fbiinit1 = ((tdvfb_cvg_read(sc, TDV_OFF_FBIINIT1) & 
	    TDV_FBIINIT1_VIDMASK) | 
	    TDV_FBIINIT1_DR_DATA |
	    TDV_FBIINIT1_DR_BLANKING |
	    TDV_FBIINIT1_DR_HVSYNC |
	    TDV_FBIINIT1_DR_DCLK |
	    TDV_FBIINIT1_IN_VCLK_2X );

	if (sc->sc_voodootype == TDV_VOODOO_2) {
		fbiinit1 |= ((sc->sc_x_tiles & 0x20) >> 5) 
		    << TDV_FBIINIT1_TILES_X_MSB | ((sc->sc_x_tiles & 0x1e) >> 1)
		    << TDV_FBIINIT1_TILES_X;
		fbiinit6 = (sc->sc_x_tiles & 0x1) << TDV_FBIINIT6_TILES_X_LSB;
	} else
		fbiinit1 |= sc->sc_x_tiles  << TDV_FBIINIT1_TILES_X;

	fbiinit1 |= TDV_FBIINIT1_VCLK_2X << TDV_FBIINIT1_VCLK_SRC;

	if (sc->sc_voodootype == TDV_VOODOO_2) {
		fbiinit5 = tdvfb_cvg_read(sc, TDV_OFF_FBIINIT5) 
		    & TDV_FBIINIT5_VIDMASK;
		if (sc->sc_videomode->flags & VID_PHSYNC)
			fbiinit5 |= TDV_FBIINIT5_PHSYNC;
		if (sc->sc_videomode->flags & VID_PVSYNC)
			fbiinit5 |= TDV_FBIINIT5_PVSYNC;
	}
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT1, fbiinit1);
	if (sc->sc_voodootype == TDV_VOODOO_2) {
		tdvfb_cvg_write(sc, TDV_OFF_FBIINIT6, fbiinit6);
		tdvfb_cvg_write(sc, TDV_OFF_FBIINIT5, fbiinit5); 
	}
	tdvfb_wait(sc);	

	/* unreset, enable DRAM refresh */
	tdvfb_cvg_unset(sc, TDV_OFF_FBIINIT1, TDV_FBIINIT1_VIDEO_RST);
	tdvfb_cvg_unset(sc, TDV_OFF_FBIINIT0, TDV_FBIINIT0_FBI_RST |
	    TDV_FBIINIT0_FIFO_RST);
	tdvfb_cvg_set(sc, TDV_OFF_FBIINIT2, TDV_FBIINIT2_DRAM_REFR);
	/* diable access to FBIINIT regs */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_FIFO);
	tdvfb_wait(sc);	

	if (sc->sc_bpp == 16)
		lfbmode = TDV_LFBMODE_565; 
	else if (sc->sc_bpp == 32)
		lfbmode = TDV_LFBMODE_8888; 
	else
		return false;

#if BYTE_ORDER == BIG_ENDIAN
	lfbmode |= TDV_LFBMODE_BSW_WR | TDV_LFBMODE_BSW_RD;
#endif

	tdvfb_cvg_write(sc, TDV_OFF_LFBMODE, lfbmode);

	return true;
}

/*
 * Update DAC parameters for selected video mode. 
 */
static void
tdvfb_videomode_dac(struct tdvfb_softc *sc)
{
	uint32_t fbiinit2, fbiinit3; 

	/* remember current FBIINIT settings */
	fbiinit2 = tdvfb_cvg_read(sc, TDV_OFF_FBIINIT2);
	fbiinit3 = tdvfb_cvg_read(sc, TDV_OFF_FBIINIT3);

	/* remap DAC */	
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT | TDV_INITENABLE_REMAPDAC);

	tdvfb_cvg_dac_write(sc, TDV_GENDAC_CMD, TDV_GENDAC_CMD_16BITS);

	tdvfb_gendac_set_vid_timing(sc, &(sc->vid_timing));

	/* disable remapping */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT);
	/* restore */
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT2, fbiinit2);
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT2, fbiinit3);
}

/* 
 * Check how much memory do we have. Actually, Voodoo1/2 has separate 
 * framebuffer and texture memory. This function only checks for framebuffer 
 * memory. Texture memory ramains unused.
 */
static size_t
tdvfb_mem_size(struct tdvfb_softc *sc)
{
	size_t mem_size;
	uint32_t vram_test4, vram_test2;

	bus_space_write_4(sc->sc_cvgt, sc->sc_fbh, 0, 0x11aabbaa);
	bus_space_write_4(sc->sc_cvgt, sc->sc_fbh, 0x100000, 0x22aabbaa);
	bus_space_write_4(sc->sc_cvgt, sc->sc_fbh, 0x200000, 0x44aabbaa);

	vram_test4 = bus_space_read_4(sc->sc_cvgt, sc->sc_fbh, 0x400000);
	vram_test2 = bus_space_read_4(sc->sc_cvgt, sc->sc_fbh, 0x200000);

	if (vram_test4 == 0x44aabbaa)
		mem_size = 4*1024*1024;
	else if (vram_test2 == 0x22aabbaa) {
		mem_size = 2*1024*1024;
	} else
		mem_size = 1*1024*1024;

	return mem_size;
}

/* do the low level init of Voodoo board */
static bool
tdvfb_init(struct tdvfb_softc *sc)
{
	/* undocumented - found in glide code */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_VCLK_DISABLE_REG, 0);
	/* allow write to hardware initialization registers */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT);

	/* reset the board */
	tdvfb_cvg_set(sc, TDV_OFF_FBIINIT1, TDV_FBIINIT1_VIDEO_RST);
	tdvfb_wait(sc);
	tdvfb_cvg_set(sc, TDV_OFF_FBIINIT0, TDV_FBIINIT0_FBI_RST | 
	    TDV_FBIINIT0_FIFO_RST);
	tdvfb_wait(sc);

	/* disable video RAM refresh */
	tdvfb_cvg_unset(sc, TDV_OFF_FBIINIT2, TDV_FBIINIT2_DRAM_REFR);
	tdvfb_wait(sc);

	/* on voodoo1 I had to read FBIINIT2 before remapping, 
	 * otherwise weird things were happening, on v2 it works just fine */
	/* tdvfb_cvg_read(sc, TDV_OFF_FBIINIT2); */

	/* remap DAC */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT | TDV_INITENABLE_REMAPDAC);

	/* detect supported DAC, TODO: we really should support other DACs */
	if(!tdvfb_gendac_detect(sc)) {
		aprint_error_dev(sc->sc_dev, "could not detect ICS GENDAC\n");
		return false;
	}

	/* calculate PLL used to drive the chips (graphics clock) */
	if(sc->sc_voodootype == TDV_VOODOO_2)
		sc->cvg_timing = tdvfb_gendac_calc_pll(TDV_CVG_CLK);
	else
		sc->cvg_timing = tdvfb_gendac_calc_pll(TDV_SST_CLK);

	/* set PLL for gfx clock */
	tdvfb_gendac_set_cvg_timing(sc, &(sc->cvg_timing));

	/* don't remap the DAC anymore */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG, 
	    TDV_INITENABLE_EN_INIT | TDV_INITENABLE_EN_FIFO);

	/* set FBIINIT registers to some default values that make sense */
	tdvfb_fbiinit_defaults(sc);

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_INITENABLE_REG,
	    TDV_INITENABLE_EN_FIFO);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, TDV_VCLK_ENABLE_REG, 0); 

	return true;
}

static void
tdvfb_fbiinit_defaults(struct tdvfb_softc *sc)
{
	uint32_t fbiinit0, fbiinit1, fbiinit2, fbiinit3, fbiinit4, fbiinit6;

	fbiinit0 = TDV_FBIINIT0_VGA_PASS; /* disable VGA passthrough */
	fbiinit1 = /*TDV_FBIINIT1_PCIWAIT |*/ /* one wait state for PCI write */
	    TDV_FBIINIT1_LFB_EN |	  /* enable lfb reads */
	    TDV_FBIINIT1_VIDEO_RST | 	  /* video timing reset */
	    10 << TDV_FBIINIT1_TILES_X |   /* tiles x/horizontal */
	    TDV_FBIINIT1_VCLK_2X << TDV_FBIINIT1_VCLK_SRC ;

	fbiinit2 = TDV_FBIINIT2_SWB_ALG |/* swap buffer use DAC sync */
	    TDV_FBIINIT2_FAST_RAS |	  /* fast RAS read */
	    TDV_FBIINIT2_DRAM_OE |	  /* enable DRAM OE */
	    TDV_FBIINIT2_DRAM_REFR |	  /* enable DRAM refresh */
	    TDV_FBIINIT2_FIFO_RDA |	  /* FIFO read ahead */
	    TDV_FBIINIT2_DRAM_REF16 << TDV_FBIINIT2_DRAM_REFLD; /* 16 ms */
	    
	fbiinit3 = TDV_FBIINIT3_TREX_DIS; /* disable texture mapping */
	
	fbiinit4 = /*TDV_FBIINIT4_PCIWAIT|*/ /* one wait state for PCI write */
	    TDV_FBIINIT4_LFB_RDA;	  /* lfb read ahead */

	fbiinit6 = 0;
#ifdef TDVFB_DEBUG 	  
	aprint_normal("fbiinit: 0 %x, 1 %x, 2 %x, 3 %x, 4 %x, 6 %x\n",
	    fbiinit0, fbiinit1, fbiinit2, fbiinit3, fbiinit4, fbiinit6);
#endif /* TDVFB_DEBUG */
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT0, fbiinit0);
	tdvfb_wait(sc);
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT1, fbiinit1);
	tdvfb_wait(sc);
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT2, fbiinit2);
	tdvfb_wait(sc);
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT3, fbiinit3);
	tdvfb_wait(sc);
	tdvfb_cvg_write(sc, TDV_OFF_FBIINIT4, fbiinit4);
	tdvfb_wait(sc);
	if (sc->sc_voodootype == TDV_VOODOO_2) {
		tdvfb_cvg_write(sc, TDV_OFF_FBIINIT6, fbiinit6);
		tdvfb_wait(sc);
	}
}

static void
tdvfb_gendac_set_vid_timing(struct tdvfb_softc *sc, 
    struct tdvfb_dac_timing *timing)
{
	uint8_t pllreg;

	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, TDV_GENDAC_PLL_CTRL);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);

	/* write the timing for gfx clock into "slot" 0 */
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLWR, TDV_GENDAC_PLL_0);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, timing->m);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, timing->n);
	/* select "slot" 0 for output */
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLWR, TDV_GENDAC_PLL_CTRL);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, 
	    (pllreg & TDV_GENDAC_VIDPLLMASK) | TDV_GENDAC_PLL_VIDCLK |
	    TDV_GENDAC_PLL_VIDCLK0);
	tdvfb_wait(sc);
	tdvfb_wait(sc);
#ifdef TDVFB_DEBUG
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, TDV_GENDAC_PLL_0);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	aprint_normal("vid read again: %d\n", pllreg);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	aprint_normal("vid read again: %d\n", pllreg);
#endif /* TDVFB_DEBUG */
}

static void
tdvfb_gendac_set_cvg_timing(struct tdvfb_softc *sc, 
    struct tdvfb_dac_timing *timing)
{
	uint8_t pllreg;

	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, TDV_GENDAC_PLL_CTRL);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);

	/* write the timing for gfx clock into "slot" A */
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLWR, TDV_GENDAC_PLL_A);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, timing->m);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, timing->n);
	/* select "slot" A for output */
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLWR, TDV_GENDAC_PLL_CTRL);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLDATA, 
	    (pllreg & TDV_GENDAC_CVGPLLMASK) | TDV_GENDAC_PLL_CVGCLKA);
#ifdef TDVFB_DEBUG
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, TDV_GENDAC_PLL_A);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	aprint_normal("read again: %d\n", pllreg);
	pllreg = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	aprint_normal("read again: %d\n", pllreg);
#endif /* TDVFB_DEBUG */
	tdvfb_wait(sc);
}

static struct tdvfb_dac_timing 
tdvfb_gendac_calc_pll(int freq) 
{
	int n1, n2;
	int m, mdbl;
	int best_m, best_n1, best_error;
	int fout;
	struct tdvfb_dac_timing timing;

	best_m = -1; best_n1 = -1;

	/* select highest possible n2, check n2 * fCLK < TDV_GENDAC_MAXVCO */
	for (n2 = TDV_GENDAC_MAX_N2; n2 >= TDV_GENDAC_MIN_N2; n2--) {
		if ((freq * (1 << n2)) < TDV_GENDAC_MAXVCO) 
			break;
	}

	best_error = freq;

	/* 
	 * m+2	    2^n2 * fOUT
	 * ----  =  -----------
	 * n1+2        fREF
	 */
	for (n1 = TDV_GENDAC_MIN_N1; n1 <= TDV_GENDAC_MAX_N1; n1++) {
		/* loop mostly inspired by Linux driver */
		mdbl = (2 * freq * (1 << n2)*(n1 + 2)) / TDV_GENDAC_REFFREQ - 4;
		if (mdbl % 2)
			m = mdbl/2+1;
		else
			m = mdbl/2;
			
		if(m > TDV_GENDAC_MAX_M)
			break;

		fout = (TDV_GENDAC_REFFREQ * (m + 2)) / ((1 << n2) * (n1 + 2));
		if ((abs(fout - freq) < best_error) && (m > 0)) {
			best_n1 = n1;
			best_m = m;
			best_error = abs(fout - freq);
			if (200*best_error < freq) break;
		}

	}

	fout = (TDV_GENDAC_REFFREQ * (best_m + 2)) / ((1 << n2) * (best_n1 + 2));
	timing.m = best_m;
	timing.n = (n2 << 5) | best_n1;
	timing.fout = fout;

#ifdef TDVFB_DEBUG
	aprint_normal("tdvfb_gendac_calc_pll ret: m %d, n %d, fout %d kHz\n", 
	    timing.m, timing.n, timing.fout);
#endif /* TDVFB_DEBUG */

	return timing; 
}

static bool
tdvfb_gendac_detect(struct tdvfb_softc *sc)
{
	uint8_t m_f1, m_f7, m_fb;
	uint8_t n_f1, n_f7, n_fb; 

	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, 0x1);
	m_f1 = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	n_f1 = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, 0x7);
	m_f7 = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	n_f7 = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	tdvfb_cvg_dac_write(sc, TDV_GENDAC_PLLRD, 0xB);
	m_fb = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);
	n_fb = tdvfb_cvg_dac_read(sc, TDV_GENDAC_PLLDATA);

	if( (m_f1 == TDV_GENDAC_DFLT_F1_M) &&
	    (n_f1 == TDV_GENDAC_DFLT_F1_N) &&
	    (m_f7 == TDV_GENDAC_DFLT_F7_M) &&
	    (n_f7 == TDV_GENDAC_DFLT_F7_N) &&
	    (m_fb == TDV_GENDAC_DFLT_FB_M) &&
	    (n_fb == TDV_GENDAC_DFLT_FB_N) ) {
		aprint_normal_dev(sc->sc_dev, "ICS 5342 GENDAC\n");
		return true;
	}

	return false;
}

static void
tdvfb_wait(struct tdvfb_softc *sc) 
{
	uint32_t x, cnt;
	cnt = 0;
	for (x = 0; x < MAXLOOP; x++) {
		if (tdvfb_cvg_read(sc, TDV_OFF_STATUS) & TDV_STATUS_FBI_BUSY)
			cnt = 0;
		else
			cnt++;

		if (cnt >= 5)	/* Voodoo2 specs suggest at least 3 */
			break;
	}

	if (x == MAXLOOP)
		/* 
		 * The console probably isn't working now anyway, so maybe 
		 * let's panic... At least it will drop into ddb if some other 
		 * device a console.
		 */
		panic("tdvfb is stuck!\n");
}

static uint32_t
tdvfb_cvg_read(struct tdvfb_softc *sc, uint32_t reg) 
{
	uint32_t rv;
	rv = bus_space_read_4(sc->sc_cvgt, sc->sc_cvgh, reg);
#ifdef TDVFB_DEBUG_REGS
	aprint_normal("cvg_read val %x from reg %x\n", rv, reg);
#endif /* TDVFB_DEBUG_REGS */
	return rv;
}

static void
tdvfb_cvg_write(struct tdvfb_softc *sc, uint32_t reg, uint32_t val)
{
#ifdef TDVFB_DEBUG_REGS
	aprint_normal("cvg_write val %x to reg %x\n", val, reg);
#endif /* TDVFB_DEBUG_REGS */
	bus_space_write_4(sc->sc_cvgt, sc->sc_cvgh, reg, val);
}

static void
tdvfb_cvg_set(struct tdvfb_softc *sc, uint32_t reg, uint32_t bits)
{
	uint32_t v;
	v = tdvfb_cvg_read(sc, reg) | bits;
	tdvfb_cvg_write(sc, reg, v);
}

static void
tdvfb_cvg_unset(struct tdvfb_softc *sc, uint32_t reg, uint32_t bits)
{
	uint32_t v;
	v = tdvfb_cvg_read(sc, reg) & ~bits;
	tdvfb_cvg_write(sc, reg, v);
}

static uint8_t
tdvfb_cvg_dac_read(struct tdvfb_softc *sc, uint32_t reg)
{
	uint32_t rv;

	tdvfb_cvg_dac_write(sc, reg, TDV_DAC_DATA_READ);

	rv = tdvfb_cvg_read(sc, TDV_OFF_DAC_READ);
#ifdef TDVFB_DEBUG_REGS
	aprint_normal("cvg_dac_read val %x from reg %x\n", rv, reg);
#endif /* TDVFB_DEBUG_REGS */
	return rv & 0xFF;
}

static void
tdvfb_cvg_dac_write(struct tdvfb_softc *sc, uint32_t reg, uint32_t val)
{
	uint32_t wreg;

	wreg = ((reg & TDV_GENDAC_ADDRMASK) << 8) | val;

#ifdef TDVFB_DEBUG_REGS
	aprint_normal("cvg_dac_write val %x to reg %x (%x)\n", val, reg,
	    wreg);
#endif /* TDVFB_DEBUG_REGS */

	tdvfb_cvg_write(sc, TDV_OFF_DAC_DATA, wreg);
	tdvfb_wait(sc);
}

static void
tdvfb_rectfill(struct tdvfb_softc *sc, int x, int y, int wi, int he,
    uint32_t color)
{
	tdvfb_cvg_write(sc, TDV_OFF_BLTSRC, 0);
	tdvfb_cvg_write(sc, TDV_OFF_BLTDST, 0);
	tdvfb_cvg_write(sc, TDV_OFF_BLTROP, TDV_BLTROP_COPY);
	tdvfb_cvg_write(sc, TDV_OFF_BLTXYSTRIDE, 
	    sc->sc_linebytes | (sc->sc_linebytes << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTDSTXY, x | (y << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTSIZE, wi | (he << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTCMD, TDV_BLTCMD_RECTFILL | 
	    TDV_BLTCMD_LAUNCH | TDV_BLTCMD_FMT_565 << 3 | TDV_BLTCMD_DSTTILED |
	    TDV_BLTCMD_CLIPRECT );
	tdvfb_wait(sc);
}

static void
tdvfb_bitblt(struct tdvfb_softc *sc, int xs, int ys, int xd, int yd, int wi,
    int he) 
{
	tdvfb_cvg_write(sc, TDV_OFF_BLTSRC, 0);
	tdvfb_cvg_write(sc, TDV_OFF_BLTDST, 0);
	tdvfb_cvg_write(sc, TDV_OFF_BLTROP, TDV_BLTROP_COPY);
	tdvfb_cvg_write(sc, TDV_OFF_BLTXYSTRIDE, 
	    sc->sc_linebytes | (sc->sc_linebytes << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTSRCXY, xs | (ys << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTDSTXY, xd | (yd << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTSIZE, wi | (he << 16));
	tdvfb_cvg_write(sc, TDV_OFF_BLTCMD, TDV_BLTCMD_SCR2SCR | 
	    TDV_BLTCMD_LAUNCH | TDV_BLTCMD_FMT_565 << 3);
	
	tdvfb_wait(sc);
}

static void
tdvfb_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct tdvfb_softc *sc;
	struct rasops_info *ri;
	struct vcons_screen *scr;
	int x, ys, yd, wi, he;

	ri = cookie;
	scr = ri->ri_hw;
	sc = scr->scr_cookie;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		x = ri->ri_xorigin;
		ys = ri->ri_yorigin + ri->ri_font->fontheight * srcrow;
		yd = ri->ri_yorigin + ri->ri_font->fontheight * dstrow;
		wi = ri->ri_emuwidth;
		he = ri->ri_font->fontheight * nrows;
		tdvfb_bitblt(sc, x, ys, x, yd, wi, he);
	}
}

static void
tdvfb_eraserows(void *cookie, int row, int nrows, long fillattr)
{

	struct tdvfb_softc *sc;
	struct rasops_info *ri;
	struct vcons_screen *scr;
	int x, y, wi, he, fg, bg, ul;

	ri = cookie;
	scr = ri->ri_hw;
	sc = scr->scr_cookie;

	if (sc->sc_mode == WSDISPLAYIO_MODE_EMUL) {
		rasops_unpack_attr(fillattr, &fg, &bg, &ul);
		if ((row == 0) && (nrows == ri->ri_rows)) 
			tdvfb_rectfill(sc, 0, 0, ri->ri_width,
			    ri->ri_height, ri->ri_devcmap[bg]);
		else {
			x = ri->ri_xorigin;
			y = ri->ri_yorigin + ri->ri_font->fontheight * row;
			wi = ri->ri_emuwidth;
			he = ri->ri_font->fontheight * nrows;
			tdvfb_rectfill(sc, x, y, wi, he, ri->ri_devcmap[bg]);
		}
	}
}

static int
tdvfb_ioctl(void *v, void *vs, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct vcons_data *vd;
	struct tdvfb_softc *sc;
	struct wsdisplay_fbinfo *wsfbi;
	struct vcons_screen *ms;

	vd = v;
	sc = vd->cookie;
	ms = vd->active;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIMISC;
		return 0;

	case PCI_IOC_CFGREAD:
	case PCI_IOC_CFGWRITE:
		return pci_devioctl(sc->sc_pc, sc->sc_pcitag,
		    cmd, data, flag, l);

	case WSDISPLAYIO_GET_BUSID:
		return wsdisplayio_busid_pci(sc->sc_dev, sc->sc_pc,
		    sc->sc_pcitag, data);

	case WSDISPLAYIO_GINFO:
		if (ms == NULL)
			return ENODEV;

		wsfbi = (void*) data;
		wsfbi->height = ms->scr_ri.ri_height;
		wsfbi->width = ms->scr_ri.ri_width;
		wsfbi->depth = ms->scr_ri.ri_depth;	
		wsfbi->cmsize = 256;
		return 0;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int*)data = sc->sc_linebytes;
		return 0;
	
	case WSDISPLAYIO_SMODE: 
		{
			int new_mode = *(int*)data;
			if (new_mode != sc->sc_mode) {
				sc->sc_mode = new_mode;
				if(new_mode == WSDISPLAYIO_MODE_EMUL) 
					vcons_redraw_screen(ms);
			}		
			return 0;
		}
	case WSDISPLAYIO_GET_FBINFO:
		{
			struct wsdisplayio_fbinfo *fbi = data;
			struct rasops_info *ri;
			int ret;

			ri = &sc->vd.active->scr_ri;
			ret = wsdisplayio_get_fbinfo(ri, fbi);
			return ret;
		}
	}	
	return EPASSTHROUGH;	
}

static paddr_t
tdvfb_mmap(void *v, void *vs, off_t offset, int prot)
{
	struct vcons_data *vd;
	struct tdvfb_softc *sc;
	paddr_t pa;

	vd = v;
	sc = vd->cookie;

	if (offset < sc->sc_memsize) {
		pa = bus_space_mmap(sc->sc_cvgt, sc->sc_fbh + offset, 0, prot,
		    BUS_SPACE_MAP_LINEAR);
		return pa;
	}

	return -1;
}

