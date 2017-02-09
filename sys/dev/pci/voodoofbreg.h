/*	$NetBSD: voodoofbreg.h,v 1.4 2012/01/21 16:12:57 jakllsch Exp $	*/

/*
 * Copyright 2005, 2006 by Michael Lorenz.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Kevin E. Martin not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Kevin E. Martin
 * makes no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * KEVIN E. MARTIN, RICKARD E. FAITH, AND TIAGO GONS DISCLAIM ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
*/

/* 
 * stolen wholesale from Andreas Drewke's (andreas_dr@gmx.de) Voodoo3 driver
 * for BeOS 
 */

#ifndef VOODOOFB_H
#define VOODOOFB_H

/* membase0 register offsets */
#define STATUS		0x00
#define PCIINIT0	0x04
#define SIPMONITOR	0x08
#define LFBMEMORYCONFIG	0x0c
#define MISCINIT0	0x10
#define MISCINIT1	0x14
#define DRAMINIT0	0x18
#define DRAMINIT1	0x1c
#define AGPINIT		0x20
#define TMUGBEINIT	0x24
#define VGAINIT0	0x28
#define VGAINIT1	0x2c
#define DRAMCOMMAND	0x30
#define DRAMDATA	0x34
/* reserved             0x38 */
/* reserved             0x3c */
#define PLLCTRL0	0x40	/* video clock */
#define PLLCTRL1	0x44	/* memory clock */

/* PLL ctrl 0 and 1 registers:
 * freq = (( N + 2 ) * Fref) / (( M + 2 ) * ( 2^K ))
 * with Fref usually 14.31818MHz
 * N: REG & 0xff00
 * M: REG & 0xfc
 * K: REG & 0x3
 */

#define PLLCTRL2	0x48	/* test modes for AGP */

#define DACMODE		0x4c
#define DAC_MODE_1_2 		0x1	/* DAC in 2:1 mode. 1:1 mode when 0 */
#define DAC_MODE_LOCK_VSYNC	0x02	/* lock vsync */
#define DAC_MODE_VSYNC_VAL	0x04	/* vsync output when locked */
#define DAC_MODE_LOCK_HSYNC	0x08	/* lock hsync */
#define DAC_MODE_HSYNC_VAL	0x10	/* hsync output when locked */

#define DACADDR		0x50
#define DACDATA		0x54
#define RGBMAXDELTA	0x58
#define VIDPROCCFG	0x5c
#define HWCURPATADDR	0x60
#define HWCURLOC	0x64
#define HWCURC0		0x68
#define HWCURC1		0x6c
#define VIDINFORMAT	0x70
#define VIDINSTATUS	0x74
#define VIDSERPARPORT	0x78
/* i2c stuff */
#define VSP_TVOUT_RESET	0x80000000	/* 0 forces TVout reset */
#define VSP_GPIO2_IN	0x40000000
#define VSP_GPIO1_OUT	0x20000000
#define VSP_VMI_RESET_N	0x10000000	/* 0 forces a VMI reset */
#define VSP_SDA1_IN		0x08000000	/* i2c bus on the feature connector */
#define VSP_SCL1_IN		0x04000000
#define VSP_SDA1_OUT	0x02000000
#define VSP_SCL1_OUT	0x01000000
#define VSP_ENABLE_IIC1	0x00800000	/* 1 enables I2C bus 1 */
#define VSP_SDA0_IN		0x00400000	/* i2c bus on the monitor connector */
#define VSP_SCL0_IN		0x00200000
#define VSP_SDA0_OUT	0x00100000
#define VSP_SCL0_OUT	0x00080000
#define VSP_ENABLE_IIC0	0x00040000	/* 1 enables I2C bus 0 */
#define VSP_VMI_ADDRESS	0x0003c000	/* mask */
#define VSP_VMI_DATA	0x00003fc0	/* mask */
#define VSP_VMI_DISABLE	0x00000020	/* 0 enables VMI output */
#define VSP_VMI_RDY_N	0x00000010
#define VSP_RW_N		0x00000008
#define VSP_DS_N		0x00000004
#define VSP_CS_N		0x00000002
#define VSP_HOST_ENABLE	0x00000001	/* 1 enables VMI host control*/

#define VIDINXDELTA	0x7c
#define VIDININITERR	0x80
#define VIDINYDELTA	0x84
#define VIDPIXBUFTHOLD	0x88
#define VIDCHRMIN	0x8c
#define VIDCHRMAX	0x90
#define VIDCURLIN	0x94
#define VIDSCREENSIZE	0x98
#define VIDOVRSTARTCRD	0x9c
#define VIDOVRENDCRD	0xa0
#define VIDOVRDUDX	0xa4
#define VIDOVRDUDXOFF	0xa8
#define VIDOVRDVDY	0xac
/*  ... */

#define VIDOVRDVDYOFF	0xe0
#define VIDDESKSTART	0xe4
#define VIDDESKSTRIDE	0xe8
/*
 * desktop and overlay strides in pixels
 * desktop stride: reg & 0x00007fff
 * overlay stride: reg & 0x7fff0000
 */

#define VIDINADDR0	0xec
#define VIDINADDR1	0xf0
#define VIDINADDR2	0xf4
#define VIDINSTRIDE	0xf8
#define VIDCUROVRSTART	0xfc
#define VIDOVERLAYSTARTCOORDS 0x9c
#define VIDOVERLAYENDSCREENCOORDS 0xa0
#define VIDOVERLAYDUDX 0xa4
#define VIDOVERLAYDUDXOFFSETSRCWIDTH 0xa8
#define VIDOVERLAYDVDY 0xac
#define VIDOVERLAYDVDYOFFSET 0xe0

#define SST_3D_OFFSET           	0x200000
#define SST_3D_LEFTOVERLAYBUF		SST_3D_OFFSET+0x250

#define V3_STATUS	(0x00100000)
#define INTCTRL		(0x00100000 + 0x04)
#define CLIP0MIN	(0x00100000 + 0x08)
#define CLIP0MAX	(0x00100000 + 0x0c)
#define DSTBASE		(0x00100000 + 0x10)
#define DSTFORMAT	(0x00100000 + 0x14)
	#define		FMT_STRIDE_MASK		0x00003fff
	#define		FMT_MONO		0x00000000
	#define		FMT_8BIT		0x00010000
	#define		FMT_16BIT		0x00030000
	#define		FMT_24BIT		0x00040000
	#define		FMT_32BIT		0x00050000
	#define		FMT_422YUYV		0x00080000
	#define		FMT_422UYVY		0x00090000
	#define		FMT_PAD_STRIDE		0x00000000
	#define		FMT_PAD_BYTE		0x00400000
	#define		FMT_PAD_WORD		0x00800000
	#define		FMT_PAD_LONG		0x00c00000
#define SRCBASE		(0x00100000 + 0x34)
#define COMMANDEXTRA_2D	(0x00100000 + 0x38)
#define CLIP1MIN	(0x00100000 + 0x4c)
#define CLIP1MAX	(0x00100000 + 0x50)
#define SRCFORMAT	(0x00100000 + 0x54)
#define SRCSIZE		(0x00100000 + 0x58)
#define SRCXY		(0x00100000 + 0x5c)
#define COLORBACK	(0x00100000 + 0x60)
#define COLORFORE	(0x00100000 + 0x64)
#define DSTSIZE		(0x00100000 + 0x68)
#define DSTXY		(0x00100000 + 0x6c)
#define COMMAND_2D	(0x00100000 + 0x70)
/*
 * ROP0 		: reg & 0xff000000
 * select clip 1	: 0x00800000
 * Y pattern offset	: 0x00700000
 * X pattern offset	: 0x000e0000
 * mono transparent	: 0x00010000
 * pattern expand	: 0x00002000
 * stipple line		: 0x00001000
 * adjust dstx		: 0x00000800	xdst will contain xdst+xwidth
 * adjust dsty		: 0x00000400
 * line reversible	: 0x00000200
 * start now		: 0x00000100	run immediately instead of wait for launch area
 * command		: 0x0000000f
 */

#define LAUNCH_2D	(0x00100000 + 0x80)

#define COMMAND_3D	(0x00200000 + 0x120)

/* register bitfields (not all, only as needed) */

#define BIT(x) (1UL << (x))

/* COMMAND_2D reg. values */
#define ROP_COPY	0xccU    // src
#define ROP_INVERT	0x55U    // NOT dst
#define ROP_XOR		0x66U    // src XOR dst

#define AUTOINC_DSTX                    BIT(10)
#define AUTOINC_DSTY                    BIT(11)
#define COMMAND_2D_FILLRECT		0x05
#define COMMAND_2D_S2S_BITBLT		0x01      // screen to screen
#define COMMAND_2D_H2S_BITBLT           0x03       // host to screen
#define SST_2D_GO						BIT(8)

#define COMMAND_3D_NOP			0x00
#define STATUS_RETRACE			BIT(6)
#define STATUS_BUSY			BIT(9)
#define MISCINIT1_CLUT_INV		BIT(0)
#define MISCINIT1_2DBLOCK_DIS		BIT(15)
#define DRAMINIT0_SGRAM_NUM		BIT(26)
#define DRAMINIT0_SGRAM_TYPE		BIT(27)
#define DRAMINIT1_MEM_SDRAM		BIT(30)
#define VGAINIT0_VGA_DISABLE		BIT(0)
#define VGAINIT0_EXT_TIMING		BIT(1)
#define VGAINIT0_8BIT_DAC		BIT(2)
#define VGAINIT0_EXT_ENABLE		BIT(6)
#define VGAINIT0_WAKEUP_3C3		BIT(8)
#define VGAINIT0_LEGACY_DISABLE		BIT(9)
#define VGAINIT0_ALT_READBACK		BIT(10)
#define VGAINIT0_FAST_BLINK		BIT(11)
#define VGAINIT0_EXTSHIFTOUT		BIT(12)
#define VGAINIT0_DECODE_3C6		BIT(13)
#define VGAINIT0_SGRAM_HBLANK_DISABLE	BIT(22)
#define VGAINIT1_MASK			0x1fffff
#define VIDCFG_VIDPROC_ENABLE		BIT(0)
#define VIDCFG_CURS_X11			BIT(1)
#define VIDCFG_HALF_MODE		BIT(4)
#define VIDCFG_CHROMA_KEY		BIT(5)
#define VIDCFG_CHROMA_KEY_INVERSION	BIT(6)
#define VIDCFG_DESK_ENABLE		BIT(7)
#define VIDCFG_OVL_ENABLE		BIT(8)
#define VIDCFG_OVL_NOT_VIDEO_IN	BIT(9)
#define VIDCFG_CLUT_BYPASS		BIT(10)
#define VIDCFG_OVL_CLUT_BYPASS	BIT(11)
#define VIDCFG_OVL_HSCALE		BIT(14)
#define VIDCFG_OVL_VSCALE		BIT(15)
#define VIDCFG_OVL_FILTER_SHIFT	16
#define VIDCFG_OVL_FILTER_POINT	0
#define VIDCFG_OVL_FILTER_2X2	1
#define VIDCFG_OVL_FILTER_4X4	2
#define VIDCFG_OVL_FILTER_BILIN	3
#define VIDCFG_OVL_FMT_SHIFT	21
#define VIDCFG_OVL_FMT_RGB565	1
#define VIDCFG_OVL_FMT_YUV411	4
#define VIDCFG_OVL_FMT_YUYV422	5
#define VIDCFG_OVL_FMT_UYVY422	6
#define VIDCFG_OVL_FMT_RGB565_DITHER 7

#define VIDCFG_2X		BIT(26)
#define VIDCFG_HWCURSOR_ENABLE	BIT(27)
#define VIDCFG_PIXFMT_SHIFT	18
#define DACMODE_2X		BIT(0)
#define VIDPROCCFGMASK          0xa2e3eb6c
#define VIDPROCDEFAULT		134481025

#define VIDCHROMAMIN 		0x8c
#define VIDCHROMAMAX 		0x90
#define VIDDESKTOPOVERLAYSTRIDE 0xe8

#define CRTC_INDEX	0x3d4
#define CRTC_DATA	0x3d5
#define SEQ_INDEX	0x3c4
#define SEQ_DATA	0x3c5
#define MISC_W		0x3c2
	#define		VSYNC_NEG	0x80
	#define		HSYNC_NEG	0x40
#define GRA_INDEX	0x3ce
#define GRA_DATA	0x3cf
#define ATT_IW		0x3c0
#define IS1_R		0x3da

/* CRTC registers */
#define CRTC_HTOTAL		0	/* lower 8 bit of display width in chars -5 */
#define CRTC_HDISP_ENABLE_END	1	/* no. of visible chars per line -1 */
#define CRTC_HDISP_BLANK_START	2	/* characters per line before blanking */
#define CRTC_HDISP_BLANK_END	3	/* no. o blank chars, skew, compatibility read */
#define CRTC_HDISP_SYNC_START	4	/* character count when sync becomes active */
#define CRTC_HDISP_SYNC_END	5	/* sync end, skew, blank end */
#define CRTC_VDISP_TOTAL	6	/* number of scanlines -2 */
#define CRTC_OVERFLOW		7	/* various overflow bits */
#define CRTC_PRESET_ROW_SCAN	8	/* horizontal soft scrolling in character mode */
#define CRTC_MAX_SCAN_LINE	9	/* scanlines per character */
#define CRTC_CURSOR_START	10	/* text cursor start line */
#define CRTC_CURSOR_END		11	/* text cursor end line */
#define CRTC_SCREEN_START_HIGH	12	/* offset in display memory */
#define CRTC_SCREEN_START_LOW	13
#define CRTC_CURSOR_POS_HIGH	14
#define CRTC_CURSOR_POS_LOW	15
#define CRTC_VSYNC_START	16
#define CRTC_VSYNC_END		17
#define CRTC_VDISP_ENABLE_END	18
#define CRTC_OFFSET		19	/* textmode stride */
#define CRTC_UNDERLINE_LOC	20
#define CRTC_VDISP_BLANK_START	21
#define CRTC_VDISP_BLANK_END	22
#define CRTC_MODE_CONTROL	23
#define CRTC_LINE_COMPARE	24
#define CRTC_HDISP_EXT		26
#define CRTC_VDISP_EXT		27
#define CRTC_PCI_READBACK	28
#define CRTC_SCRATCH_1		29
#define CRTC_SCRATCH_2		30
#define CRTC_SCRATCH_3		31
#define CRTC_VDISP_PRELOAD_LOW	32
#define CRTC_VDISP_PRELOAD_HIGH	33
#define CRTC_LATCHES_READBACK	34
#define CRTC_ATTR_READBACK	36	/* bit 7 = 0 : attr. ctrlr reads index, 1 -> data */
#define CRTC_ATTR_INDEX		38

#endif
