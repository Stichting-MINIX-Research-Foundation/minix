/* $NetBSD: radeonfbvar.h,v 1.20 2014/11/05 19:39:17 macallan Exp $ */

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 

/*
 * ATI Technologies Inc. ("ATI") has not assisted in the creation of, and
 * does not endorse, this software.  ATI will not be responsible or liable
 * for any actual or alleged damage or loss caused by or in connection with
 * the use of or reliance on this software.
 */

#ifndef _DEV_PCI_RADEONFBVAR_H
#define	_DEV_PCI_RADEONFBVAR_H

#include "opt_splash.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <dev/pci/pcivar.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wsfont/wsfont.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>
#include <dev/wscons/wsdisplay_glyphcachevar.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#ifdef SPLASHSCREEN
#include <dev/splash/splash.h>
#endif
#include <dev/i2c/i2cvar.h>

/* XXX: change this when we complete the support for multi HEAD */
#define	RADEON_NDISPLAYS	(1)
#define	RADEON_MAXX		(2048)
#define	RADEON_MAXY		(1536)
#define	RADEON_MAXBPP		(32)
#define	RADEON_STRIDEALIGN	(64)
#define	RADEON_CURSORMAXX	(64)
#define	RADEON_CURSORMAXY	(64)
#define	RADEON_PANINCREMENT	(1)

struct radeonfb_softc;

struct radeonfb_port {
	int			rp_number;
	int			rp_mon_type;
	int			rp_conn_type;
	int			rp_dac_type;
	int			rp_ddc_type;
	int			rp_tmds_type;
	int			rp_edid_valid;
	struct edid_info	rp_edid;
};

/* connector values used by legacy bios */
#define	RADEON_CONN_NONE	0
#define	RADEON_CONN_PROPRIETARY	1	/* think LVDS ribbon cable */
#define	RADEON_CONN_CRT		2
#define	RADEON_CONN_DVI_I	3
#define	RADEON_CONN_DVI_D	4
#define	RADEON_CONN_CTV		5
#define	RADEON_CONN_STV		6
#define	RADEON_CONN_UNSUPPORTED	7

/* connector values used by atom bios */
#define	ATOM_CONN_NONE		0
#define	ATOM_CONN_VGA		1
#define	ATOM_CONN_DVI_I		2
#define	ATOM_CONN_DVI_D		3
#define	ATOM_CONN_DVI_A		4
#define	ATOM_CONN_STV		5
#define	ATOM_CONN_CTV		6
#define	ATOM_CONN_LVDS		7
#define	ATOM_CONN_DIGITAL	8
#define	ATOM_CONN_UNSUPPORTED	9

#define	RADEON_DDC_NONE		0
#define	RADEON_DDC_MONID	1
#define	RADEON_DDC_DVI		2
#define	RADEON_DDC_VGA		3
#define	RADEON_DDC_CRT2		4

#define	RADEON_DAC_UNKNOWN	-1
#define	RADEON_DAC_PRIMARY	0
#define	RADEON_DAC_TVDAC	1

#define	RADEON_TMDS_UNKNOWN	-1
#define	RADEON_TMDS_INT		0
#define	RADEON_TMDS_EXT		1

#define	RADEON_MT_UNKNOWN	-1
#define	RADEON_MT_NONE		0
#define	RADEON_MT_CRT		1
#define	RADEON_MT_LCD		2	/* LVDS */
#define	RADEON_MT_DFP		3	/* TMDS */
#define	RADEON_MT_CTV		4
#define	RADEON_MT_STV		5

struct radeonfb_i2c {
	struct radeonfb_softc	*ric_softc;
	int			ric_register;
	struct i2c_controller	ric_controller;
};

struct radeonfb_crtc {
	int			rc_number;
	struct videomode	rc_videomode;
	uint16_t		rc_xoffset;
	uint16_t		rc_yoffset;

	struct radeonfb_port	*rc_port;
};

struct radeonfb_cursor {
	int			rc_visible;
	struct wsdisplay_curpos	rc_pos;
	struct wsdisplay_curpos	rc_hot;
	struct wsdisplay_curpos	rc_size;
	uint32_t		rc_cmap[2];
	uint8_t			rc_image[512];	/* 64x64x1 bit */
	uint8_t			rc_mask[512];	/* 64x64x1 bit */
};

struct radeonfb_display {
	struct radeonfb_softc	*rd_softc;
	int			rd_number;	/* 0 .. RADEON_NDISPLAYS */

	bus_size_t		rd_offset;	/* offset within FB memory */
	vaddr_t			rd_fbptr;	/* framebuffer pointer */
	vaddr_t			rd_curptr;	/* cursor data pointer */
	size_t			rd_curoff;	/* cursor offset */

	uint16_t		rd_bpp;
	uint16_t		rd_virtx;
	uint16_t		rd_virty;
	uint16_t		rd_stride;
	uint16_t		rd_format;	/* chip pixel format */

	uint16_t		rd_xoffset;
	uint16_t		rd_yoffset;

	uint32_t		rd_bg;		/* background */
	bool			rd_console;

	struct callout          rd_bl_lvds_co;  /* delayed lvds operation */
	uint32_t                rd_bl_lvds_val; /* value of delayed lvds */
	int			rd_bl_on;
	int			rd_bl_level;

	int			rd_wsmode;

	int			rd_ncrtcs;
	struct radeonfb_crtc	rd_crtcs[2];

	struct radeonfb_cursor	rd_cursor;
	/* XXX: this should probaby be an array for CRTCs */
	//struct videomode	rd_videomode;

	struct wsscreen_list	rd_wsscreenlist;
	struct wsscreen_descr	rd_wsscreens_storage[1];
	struct wsscreen_descr	*rd_wsscreens;
	struct vcons_screen	rd_vscreen;
	struct vcons_data	rd_vd;
	void (*rd_putchar)(void *, int, int, u_int, long);
	glyphcache		rd_gc;

	uint8_t			rd_cmap_red[256];
	uint8_t			rd_cmap_green[256];
	uint8_t			rd_cmap_blue[256];

#ifdef SPLASHSCREEN
	struct splash_info	rd_splash;
#endif
};

struct radeon_tmds_pll {
	uint32_t		rtp_freq;
	uint32_t		rtp_pll;
};

struct radeonfb_softc {
	device_t		sc_dev;
	uint16_t		sc_family;
	uint16_t		sc_flags;
	pcireg_t		sc_id;

	bus_space_tag_t		sc_regt;
	bus_space_handle_t	sc_regh;
	bus_size_t		sc_regsz;
	bus_addr_t		sc_regaddr;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_memsz;
	bus_addr_t		sc_memaddr;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_iosz;
	bus_addr_t		sc_ioaddr;

	int			sc_needs_unmap;
	int			sc_mapped;

	/* size of a single display */
	int			sc_maxx;
	int			sc_maxy;
	int			sc_maxbpp;
	int			sc_fboffset;
	int			sc_fbsize;

	bus_space_tag_t		sc_romt;
	bus_space_handle_t	sc_romh;
	bus_size_t		sc_romsz;
	bus_addr_t		sc_romaddr;
	bus_space_handle_t	sc_biosh;

	bus_dma_tag_t		sc_dmat;

	uint16_t		sc_refclk;
	uint16_t		sc_refdiv;
	uint32_t		sc_minpll;
	uint32_t		sc_maxpll;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_pt;

	/* card's idea of addresses, internally */
	uint32_t		sc_aperbase;

	int			sc_ndisplays;
	struct radeonfb_display	sc_displays[RADEON_NDISPLAYS];

	int			sc_nports;
	struct radeonfb_port	sc_ports[2];

	struct radeon_tmds_pll	sc_tmds_pll[4];

	struct radeonfb_i2c	sc_i2c[4];

	uint8_t			*sc_bios;
	bus_size_t		sc_biossz;
	uint32_t		sc_fp_gen_cntl;

	char			sc_modebuf[64];
	const char		*sc_defaultmode;
};

/* chip families */
#define	RADEON_R100	1
#define	RADEON_RV100	2
#define	RADEON_RS100	3
#define	RADEON_RV200	4
#define	RADEON_RS200	5
#define	RADEON_R200	6
#define	RADEON_RV250	7
#define	RADEON_RS300	8
#define	RADEON_RV280	9
#define	RADEON_R300	10
#define	RADEON_R350	11
#define	RADEON_RV350	12
#define	RADEON_RV380	13
#define	RADEON_R420	14
#define	RADEON_FAMILIES	15

/* feature flags */
#define	RFB_MOB		(1 << 0)	/* Mobility */
#define	RFB_NCRTC2	(1 << 1)	/* No CRTC2 */
#define	RFB_IGP		(1 << 2)
#define	RFB_R300CG	(1 << 3)
#define	RFB_SDAC	(1 << 4)	/* Single DAC */
#define	RFB_R300	(1 << 5)	/* R300 variants -- newer parts */
#define	RFB_RV100	(1 << 6)	/* RV100 variants -- previous gen */
#define	RFB_ATOM	(1 << 7)	/* ATOM bios */
#define RFB_INV_BLIGHT	(1 << 8)	/* backlight level inverted */

#define	IS_MOBILITY(sc)	((sc)->sc_flags & RFB_MOB)
#define	HAS_CRTC2(sc)	(((sc)->sc_flags & RFB_NCRTC2) == 0)

#define	IS_R300(sc)	((sc)->sc_flags & RFB_R300)
#define	HAS_R300CG(sc)	((sc)->sc_flags & RFB_R300CG)
#define	HAS_SDAC(sc)	((sc)->sc_flags & RFB_SDAC)
#define	IS_RV100(sc)	((sc)->sc_flags & RFB_RV100)
#define	IS_IGP(sc)	((sc)->sc_flags & RFB_IGP)
#define	IS_ATOM(sc)	((sc)->sc_flags & RFB_ATOM)

#define	RADEON_TIMEOUT	2000000

#define	GET32(sc, r)	radeonfb_get32(sc, r)
#define	PUT32(sc, r, v)	radeonfb_put32(sc, r, v)
#define	PUT32S(sc, r, v)	radeonfb_put32s(sc, r, v)
#define	SET32(sc, r, v)	PUT32(sc, r, GET32(sc, r) | (v))
#define	CLR32(sc, r, v)	PUT32(sc, r, GET32(sc, r) & ~(v))
#define	PATCH32(sc, r, v, m)	PUT32(sc, r, (GET32(sc, r) & (m)) | (v))


#define	GETPLL(sc, r)		radeonfb_getpll(sc, r)
#define	PUTPLL(sc, r, v)	radeonfb_putpll(sc, r, v)
#define	SETPLL(sc, r, v)	PUTPLL(sc, r, GETPLL(sc, r) | (v))
#define	CLRPLL(sc, r, v)	PUTPLL(sc, r, GETPLL(sc, r) & ~(v))
#define	PATCHPLL(sc, r, v, m)	PUTPLL(sc, r, (GETPLL(sc, r) & (m)) | (v))

#define	GETROM32(sc, r)	bus_space_read_4(sc->sc_romt, sc->sc_romh, r)
#define	GETROM16(sc, r)	bus_space_read_2(sc->sc_romt, sc->sc_romh, r)
#define	GETROM8(sc, r)	bus_space_read_1(sc->sc_romt, sc->sc_romh, r)

/*
 * Some values in BIOS are misaligned...
 */
#define	GETBIOS8(sc, r)		((sc)->sc_bios[(r)])

#define	GETBIOS16(sc, r)	\
	((GETBIOS8(sc, (r) + 1) << 8) | GETBIOS8(sc, (r)))

#define	GETBIOS32(sc, r)	\
	((GETBIOS16(sc, (r) + 2) << 16) | GETBIOS16(sc, (r)))

#define	XNAME(sc)	device_xname(sc->sc_dev)

#define	DIVIDE(x,y)	(((x) + (y / 2)) / (y))

uint32_t radeonfb_get32(struct radeonfb_softc *, uint32_t);
void radeonfb_put32(struct radeonfb_softc *, uint32_t, uint32_t);
void radeonfb_put32s(struct radeonfb_softc *, uint32_t, uint32_t);
void radeonfb_mask32(struct radeonfb_softc *, uint32_t, uint32_t, uint32_t);

uint32_t radeonfb_getindex(struct radeonfb_softc *, uint32_t);
void radeonfb_putindex(struct radeonfb_softc *, uint32_t, uint32_t);
void radeonfb_maskindex(struct radeonfb_softc *, uint32_t, uint32_t, uint32_t);

uint32_t radeonfb_getpll(struct radeonfb_softc *, uint32_t);
void radeonfb_putpll(struct radeonfb_softc *, uint32_t, uint32_t);
void radeonfb_maskpll(struct radeonfb_softc *, uint32_t, uint32_t, uint32_t);

int	radeonfb_bios_init(struct radeonfb_softc *);

void	radeonfb_i2c_init(struct radeonfb_softc *);
int	radeonfb_i2c_read_edid(struct radeonfb_softc *, int, uint8_t *);

#endif	/* _DEV_PCI_RADEONFBVAR_H */
