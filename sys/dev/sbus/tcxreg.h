/*	$NetBSD: tcxreg.h,v 1.6 2014/07/16 17:58:35 macallan Exp $ */
/*
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * differences between S24 and tcx, as far as this driver is concerned:
 * - S24 has 4MB VRAM, 24bit + 2bit control planes, no expansion possible
 * - tcx has 1MB VRAM, 8bit, no control planes, may have a VSIMM that bumps
 *   VRAM to 2MB
 * - tcx can apply ROPs to STIP operations, unlike S24
 * - tcx has a Bt458 DAC, just like CG6. S24 has an AT&T 20C567
 * - the chip itself seems to be (almost) the same, just with different DACs
 *   and VRAM configuration
 */

/*
 * A TCX is composed of numerous groups of control registers, all with TLAs:
 *	DHC - ???
 *	TEC - transform engine control?
 *	THC - TEC Hardware Configuration
 *	ROM - a 128Kbyte ROM with who knows what in it.
 *	STIP - stipple engine, doesn't write attribute bits
 *	RSTIP - stipple engine, writes attribute bits
 *	BLIT - blit engine, doesn't copy attribute bits
 *	RBLIT - blit engine, does copy attribute bits
 *	ALT - ???
 *	colormap - see below
 *	frame buffer memory (video RAM)
 *	possible other stuff
 *
 *	RSTIP and RBLIT are set to size zero on my SS4's tcx, they work anyway
 *	though. No sense using them since tcx has only the lower 8bit planes,
 *	with no control planes, so there is no actual difference to STIP and
 *	BLIT ops, and things like qemu and temlib may not actually implement
 *	them.
 *	The hardware cursor registers in the THC range are cut off by the size
 *	attribute but seem to exist, although the parts that display the cursor
 *	( the DAC's overlay support ) only exist on the S24.
 * 	At this point I wouldn't be surprised if 8bit tcx actually supports
 *	the DFB24 and RDFB32 ranges, with the upper planes returning garbage.
 */

#define TCX_REG_DFB8	0
#define TCX_REG_DFB24	1
#define TCX_REG_STIP	2
#define TCX_REG_BLIT	3
#define TCX_REG_RDFB32	4
#define TCX_REG_RSTIP	5
#define TCX_REG_RBLIT	6
#define TCX_REG_TEC	7
#define TCX_REG_CMAP	8
#define TCX_REG_THC	9
#define TCX_REG_ROM	10
#define TCX_REG_DHC	11
#define TCX_REG_ALT	12

#define TCX_NREG	13

/*
 * The S24 provides the framebuffer RAM mapped in three ways:
 * 26 bits used per pixel, in 32-bit words; the low-order 24 bits are
 * blue, green, and red values, and the other two bits select the
 * display modes, per pixel);
 * 24 bits per pixel, in 32-bit words; the high-order byte reads as
 * zero, and is ignored on writes (so the mode bits cannot be altered);
 * 8 bits per pixel, unpadded; writes to this space do not modify the
 * other 18 bits.
 */
#define TCX_CTL_8_MAPPED	0x00000000	/* 8 bits, uses color map */
#define TCX_CTL_24_MAPPED	0x01000000	/* 24 bits, uses color map */
#define TCX_CTL_24_LEVEL	0x03000000	/* 24 bits, ignores color map */
#define TCX_CTL_PIXELMASK	0x00FFFFFF	/* mask for index/level */
/*
 * The DAC actually supports other bits, for example to select between the
 * red and green plane for 8bit output. Not useful here since we can only
 * access the red plane as 8bit framebuffer.
 */

/*
 * The layout of the THC.
 */

#define THC_CONFIG	0x00000000
#define THC_SENSEBUS	0x00000080
#define THC_DELAY	0x00000090
#define THC_STRAPPING	0x00000094
#define THC_LINECOUNTER	0x0000009c
#define THC_HSYNC_START	0x000000a0
#define THC_HSYNC_END	0x000000a4
#define THC_HDISP_START	0x000000a8
#define THC_HDISP_VSYNC	0x000000ac
#define THC_HDISP_END	0x000000b0
#define THC_MISC	0x00000818
#define THC_CURSOR_POS	0x000008fc
#define THC_CURSOR_1	0x00000900 /* bitmap bit 1 */
#define THC_CURSOR_0	0x00000980 /* bitmap bit 0 */

/* bits in thc_config ??? */
#define THC_CFG_FBID		0xf0000000	/* id mask */
#define THC_CFG_FBID_SHIFT	28
#define THC_CFG_SENSE		0x07000000	/* sense mask */
#define THC_CFG_SENSE_SHIFT	24
#define THC_CFG_REV		0x00f00000	/* revision mask */
#define THC_CFG_REV_SHIFT	20
#define THC_CFG_RST		0x00008000	/* reset */

/* bits in thc_hcmisc */
#define	THC_MISC_OPENFLG	0x80000000	/* open flag (what's that?) */
#define	THC_MISC_SWERR_EN	0x20000000	/* enable SW error interrupt */
#define	THC_MISC_VSYNC_LEVEL	0x08000000	/* vsync level when disabled */
#define	THC_MISC_HSYNC_LEVEL	0x04000000	/* hsync level when disabled */
#define	THC_MISC_VSYNC_DISABLE	0x02000000	/* vsync disable */
#define	THC_MISC_HSYNC_DISABLE	0x01000000	/* hsync disable */
#define	THC_MISC_XXX1		0x00ffe000	/* unused */
#define	THC_MISC_RESET		0x00001000	/* ??? */
#define	THC_MISC_XXX2		0x00000800	/* unused */
#define	THC_MISC_VIDEN		0x00000400	/* video enable */
#define	THC_MISC_SYNC		0x00000200	/* not sure what ... */
#define	THC_MISC_VSYNC		0x00000100	/* ... these really are */
#define	THC_MISC_SYNCEN		0x00000080	/* sync enable */
#define	THC_MISC_CURSRES	0x00000040	/* cursor resolution */
#define	THC_MISC_INTEN		0x00000020	/* v.retrace intr enable */
#define	THC_MISC_INTR		0x00000010	/* intr pending / ack bit */
#define	THC_MISC_DACWAIT	0x0000000f	/* ??? */

/*
 * Partial description of TEC.
 */
struct tcx_tec {
	u_int	tec_config;	/* what's in it? */
	u_int	tec_xxx0[35];
	u_int	tec_delay;	/* */
#define TEC_DELAY_SYNC		0x00000f00
#define TEC_DELAY_WR_F		0x000000c0
#define TEC_DELAY_WR_R		0x00000030
#define TEC_DELAY_SOE_F		0x0000000c
#define TEC_DELAY_SOE_S		0x00000003
	u_int	tec_strapping;	/* */
#define TEC_STRAP_FIFO_LIMIT	0x00f00000
#define TEC_STRAP_CACHE_EN	0x00010000
#define TEC_STRAP_ZERO_OFFSET	0x00008000
#define TEC_STRAP_REFRSH_DIS	0x00004000
#define TEC_STRAP_REF_LOAD	0x00001000
#define TEC_STRAP_REFRSH_PERIOD	0x000003ff
	u_int	tec_hcmisc;	/* */
	u_int	tec_linecount;	/* */
	u_int	tec_hss;	/* */
	u_int	tec_hse;	/* */
	u_int	tec_hds;	/* */
	u_int	tec_hsedvs;	/* */
	u_int	tec_hde;	/* */
	u_int	tec_vss;	/* */
	u_int	tec_vse;	/* */
	u_int	tec_vds;	/* */
	u_int	tec_vde;	/* */
};

/* DAC registers */
#define DAC_ADDRESS	0x00000000
#define DAC_FB_LUT	0x00000004	/* palette / gamma table */
#define DAC_CONTROL_1	0x00000008
#define DAC_CURSOR_LUT	0x0000000c	/* cursor sprite colours */
#define DAC_CONTROL_2	0x00000018

#define DAC_C1_ID		0
#define DAC_C1_REVISION		1
#define DAC_C1_READ_MASK	4
#define DAC_C1_BLINK_MASK	5
#define DAC_C1_CONTROL_0	6
