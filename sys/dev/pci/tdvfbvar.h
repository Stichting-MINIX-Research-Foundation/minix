/*	$NetBSD: tdvfbvar.h,v 1.3 2012/07/29 20:31:53 rkujawa Exp $	*/

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

#ifndef TDVFBVAR_H
#define TDVFBVAR_H

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>

struct tdvfb_dac_timing {
	uint8_t m;
	uint8_t n;
	int fout;
};

struct tdvfb_softc {
	device_t sc_dev;

	int sc_voodootype;
#define TDV_VOODOO_1	1
#define TDV_VOODOO_2	2

	/* bus attachment, handles */
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	bus_space_tag_t sc_cvgt;
	bus_space_handle_t sc_cvgh;
	bus_addr_t sc_cvg_pa;

	bus_space_handle_t sc_fbh;

	/* DAC */
	struct tdvfb_dac_timing cvg_timing;
	struct tdvfb_dac_timing vid_timing;

	size_t sc_memsize;
	const struct videomode *sc_videomode;
	int sc_width, sc_height, sc_linebytes, sc_bpp;
	int sc_x_tiles;

	int sc_mode;
	uint32_t sc_bg;

	struct vcons_screen sc_console_screen;
	struct vcons_data vd;
	struct wsscreen_descr sc_defaultscreen_descr;
	const struct wsscreen_descr *sc_screens[1];
	struct wsscreen_list sc_screenlist;

	u_char sc_cmap_red[256];
	u_char sc_cmap_green[256];
	u_char sc_cmap_blue[256];

};

#endif /* TDVFBVAR_H */
