/* $NetBSD: ibm561reg.h,v 1.6 2012/10/20 13:29:53 macallan Exp $ */

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roland C. Dowdeswell of Ponte, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define IBM561_ADDR_LOW			0x00
#define IBM561_ADDR_HIGH		0x01
#define IBM561_CMD			0x02
#define IBM561_CMD_FB_WAT		0x03
#define IBM561_CMD_CMAP			0x03
#define IBM561_CMD_GAMMA		0x03

#define IBM561_CONFIG_REG1		0x0001
	#define CR1_MODEMASK		0xe0
		#define CR1_MODE_5_1_BASIC	0x00
		#define CR1_MODE_4_1_BASIC	0x20
		#define CR1_MODE_4_1_EXTENDED	0x40
		#define CR1_MODE_4_1_SUPER_EXT	0x60
		#define CR1_MODE_5_1_30BPP	0x80
		#define CR1_MODE_8_1_MODE_B	0xa0
		#define CR1_MODE_4_1_30BPP	0xc0
		#define CR1_MODE_8_1_MODE_A	0xe0
	#define CR1_OVL_MASK		0x18
		#define CR1_OVL_NONE		0x00
		#define CR1_OVL_8BPP		0x08
		#define CR1_OVL_16BPP		0x10
		#define CR1_OVL_VARIABLE	0x18
	#define CR1_WID_BITS_MASK	0x07
		#define CR1_WID_0	0x00
		#define CR1_WID_2	0x01
		#define CR1_WID_4	0x02
		#define CR1_WID_6	0x03
		#define CR1_WID_8	0x04
	
#define IBM561_CONFIG_REG2		0x0002
	#define CR2_ENABLE_VRAM_MASKING	0x20
	#define CR2_ENABLE_PLL		0x10
	#define CR2_ENABLE_CLC		0x08	/* sane handling of cursor
						 * coordinate updates, without
						 * this updates occur
						 * immediately instead of
						 * waiting for the 2nd byte */
	#define CR2_PLL_REF_SELECT	0x04	/* 0 - REFCLK, 1 - EXTCLK */
	#define CR2_PIXEL_CLOCK_SELECT	0x02	/* 0 - PLL, 1 - EXTCLK */
	#define CR2_ENABLE_RGB_OUTPUT	0x01

#define IBM561_CONFIG_REG3		0x0003
	#define CR3_ENABLE_INTERLACE	0x80
	#define CR3_SERIAL_CLK_CTRL	0x40	/* 1 - enable output */
	#define CR3_FIELD_POLARITY	0x20	/* 0 - EVEN first */
	#define CR3_ENABLE_MISR		0x10	/* diagnostic mode */
	#define CR3_CURSOR_UPDATE_ASYNC	0x08	/* don't wait for VBLANK */
	#define CR3_ENABLE_VIDEO	0x04	/* AUX video output */
	#define CR3_RGB			0x01	/* 1 - RGB, 0 - BGR */

#define IBM561_CONFIG_REG4		0x0004
	#define CR4_FB_SPLIT_WID_MASK	0x78	/* number of FB WID bits */
	#define CR4_ENABLE_SPLIT_WID	0x04
	#define CR4_SELECT_OL_WID	0x02	/* 1 - use upper 4 bit for OL */
	#define CR4_SELECT_FB_WID	0x01	/* 1 - use upper 4 bit for FB */

#define IBM561_SYNC_CNTL		0x0020
	#define SYNC_HSYNC_ENABLE	0x20
	#define SYNC_HSYNC_POLARITY	0x08	/* 1 - active high */
	#define SYNC_SYNC_ON_GREEN	0x02
	#define SYNC_ENABLE_PEDESTAL	0x01

#define IBM561_PLL_VCO_DIV		0x0021
#define IBM561_PLL_REF_REG		0x0022
#define IBM561_CURS_CNTL_REG		0x0030
	#define CURS_COLOR_3_TRANS	0x80	/* 0 - color 3, 1 - trans */
	#define CURS_SEPARATE		0x40	/* move crosshair separately */
	#define CURS_OVERLAP_MASK	0x30
	#define CURS_OVERLAP_XOR	0x00	/* XOR cursor and xhair */
	#define CURS_OVERLAP_OR		0x10
	#define CURS_OVERLAP_CURSOR	0x20	/* cursor has priority */
	#define CURS_OVERLAP_XHAIR	0x30	/* crosshair has priority */
	#define CURS_XHAIR_BLINK	0x08	/* enable xhair blinking */
	#define CURS_XHAIR_ENABLE	0x04
	#define CURS_BLINK		0x02	/* blink cursor */
	#define CURS_ENABLE		0x01	/* enable cursor */

#define IBM561_XHAIR_CONTROL_REG	0x0031
	#define XHAIR_PRIORITY		0x80	/* FILL or OUTLINE colour */
	#define XHAIR_WIDTH_MASK	0x60
	#define XHAIR_WIDTH_1		0x00
	#define XHAIR_WIDTH_3		0x20
	#define XHAIR_WIDTH_5		0x40
	#define XHAIR_WIDTH_7		0x60
	#define XHAIR_CLIP_MASK		0x18
	#define XHAIR_CLIP_NONE		0x00
	#define XHAIR_CLIP_SCISSORS	0x08	/* use scissors registers */
	#define XHAIR_CLIP_WINDOW	0x10	/* use window registers */
	#define XHAIR_CLIP_BOTH		0x18	/* window/scissor intersect */
	#define XHAIR_COLOR_MASK	0x06	/* 0 is transparent */
	#define XHAIR_EXT_PATTERN	0x01	/* enables colours, patterns */

#define IBM561_CURSOR_BLINK_RATE	0x0032
#define IBM561_CURSOR_BLINK_DUTY	0x0033
#define IBM561_HOTSPOT_REG		0x0034
#define IBM561_HOTSPOT_X_REG		0x0034
#define IBM561_HOTSPOT_Y_REG		0x0035

/* two registers each, low 8 bit first */
#define IBM561_CURSOR_X_REG		0x0036
#define IBM561_CURSOR_Y_REG		0x0038

#define IBM561_XHAIR_SCISSORS		0x0040
	/*
	 * four 16bit registers, low first
	 * X start, Y start, X end, Y end
	 */

#define IBM561_XHAIR_LOCATION		0x0048
	/* 2x 16bit, X first, lsb first */

#define IBM561_XHAIR_PATTERN_CONTROL	0x004c
#define IBM561_XHAIR_PATTERN_HORZ	0x004d
#define IBM561_XHAIR_PATTERN_VERT	0x004e

#define IBM561_VRAM_MASK_REG		0x0050

#define IBM561_DAC_CONTROL		0x005f
	#define DAC_DISABLE_OUTPUT	0x08
	#define DAC_10BIT_ENABLE	0x04	/* 0 forces bit 0 to 0 */
	#define DAC_SHUNT_ENABLE	0x02
	#define DAC_SLEW_ENABLE		0x01	/* 0 - 2.5ns, 1 - 7.5ns */

#define IBM561_CURSOR_LUT		0x0a10
	/*
	 * four blocks of 4, transparent, 1, 2, 3 each 
	 * cursor primary, cursor blink, xhair primary, xhair blink 
	 */

#define IBM561_CURSOR_BITMAP		0x2000
	/* 64x64, 2bit packed, msb first */

#define IBM561_DIV_DOTCLCK		0x0082
#define IBM561_FB_WINTYPE		0x1000
	#define FB_CLUT_SELECT_MASK	0x03c0	/* selects which 64 entry block
						 * in the CLUT to start with */
	#define FB_PIXEL_FORM_MASK	0x0030
	#define FB_PIXEL_8BIT		0x0000
	#define FB_PIXEL_12BIT		0x0010
	#define FB_PIXEL_16BIT		0x0020
	#define FB_PIXEL_24BIT		0x0030
	#define FB_BUFFER_SELECT	0x0008	/* 1 - buffer B */
	#define FB_MODE_MASK		0x0006
	#define FB_MODE_INDEXED		0x0000
	#define FB_MODE_GREYSCALE	0x0002
	#define FB_MODE_DIRECT		0x0004
	#define FB_MODE_TRUECOLOR	0x0006	/* doesn't work right for me */
	#define FB_MODE_TRANSP_ENABLE	0x0001

#define IBM561_AUXFB_WINTYPE		0x0e00
	#define AUXFB_BYPASS_GAMMA	0x04
	#define AUXFB_XHAIR_ENABLE	0x02
	#define AUXFB_TRANSPARENT	0x01	/* 0 - 0x00, 1 - 0xff */

#define IBM561_OL_WINTYPE		0x1400
	#define OL_CLUT_SELECT_MASK	0x03c0	/* selects which 64 entry block
						 * in the CLUT to start with */
	#define OL_PIXEL_FORM_MASK	0x0030
	#define OL_PIXEL_8BIT		0x0000
	#define OL_PIXEL_6_2BIT		0x0010	/* 6 overlay, 2 underlay */
	#define OL_PIXEL_4_4BIT		0x0020
	#define OL_PIXEL_4_DBL		0x0030
	#define OL_BUFFER_SELECT	0x0008	/* 1 - buffer B */
	#define OL_MODE_MASK		0x0006
	#define OL_MODE_INDEXED		0x0000
	#define OL_MODE_GREYSCALE	0x0002
	#define OL_MODE_INDIRECT	0x0004
	#define OL_MODE_DIRECT		0x0006
	#define OL_MODE_TRANSP_ENABLE	0x0001

#define IBM561_AUXOL_WINTYPE		0x0f00
	#define AUXOL_TRANSP_MASK	0x21
	#define AUXOL_TRANSP_00		0x00
	#define AUXOL_TRANSP_FF		0x01
	#define AUXOL_TRANSP_CHROMA0	0x20
	#define AUXOL_TRANSP_CHROMA1	0x21
	#define AUXOL_UNDERLAY_ENABLE	0x10
	#define AUXOL_OVERLAY_ENABLE	0x08
	#define AUXOL_BYPASS_GAMMA	0x04
	#define AUXOL_XHAIR_ENABLE	0x02
	
#define IBM561_CMAP_TABLE		0x4000
#define IBM561_RED_GAMMA_TABLE		0x3000
#define IBM561_GREEN_GAMMA_TABLE	0x3400
#define IBM561_BLUE_GAMMA_TABLE		0x3800

#define IBM561_CHROMAKEY0		0x0010
#define IBM561_CHROMAKEY1		0x0011
#define IBM561_CHROMAKEYMASK0		0x0012
#define IBM561_CHROMAKEYMASK1		0x0013

#define IBM561_WAT_SEG_REG		0x0006

#define IBM561_NCMAP_ENTRIES		1024
#define IBM561_NGAMMA_ENTRIES		256

/* we actually have 1024 of them, but I am just
 * going define a few, so this is good.
 */
#define IBM561_NWTYPES			16
