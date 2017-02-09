/*	$NetBSD: igsfbreg.h,v 1.8 2009/11/11 17:01:17 macallan Exp $ */

/*
 * Copyright (c) 2002 Valeriy E. Ushakov
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
 * Only tested on IGA 1682 in Krups JavaStation-NC.
 */
#ifndef _DEV_IC_IGSFBREG_H_
#define _DEV_IC_IGSFBREG_H_

/*
 * Magic address decoding for memory space accesses in CyberPro.
 */
#define IGS_MEM_MMIO_SELECT	0x00800000 /* memory mapped i/o */
#define IGS_MEM_BE_SELECT	0x00400000 /* endian select */

/*
 * Cursor sprite data in linear memory at IGS_EXT_SPRITE_DATA_{LO,HI}.
 * 64x64 pixels, 2bpp = 1Kb
 */
#define IGS_CURSOR_DATA_SIZE	1024


/*
 * Starting up the chip.
 */

/* Video Enable/Setup */
#define IGS_VDO			0x46e8
#define   IGS_VDO_ENABLE		0x08
#define   IGS_VDO_SETUP			0x10

/* Video Enable */
#define IGS_VSE			0x102
#define   IGS_VSE_ENABLE		0x01


/*
 * We map only 32 bytes of actual IGS registers at 0x3c0..0x3df.
 * This macro helps to define register names using their "absolute"
 * locations - it makes matching defines against docs easier.
 */
#define IGS_REG_BASE		0x3c0
#define IGS_REG_SIZE		0x020
#define IGS_REG_(x)		((x) - IGS_REG_BASE)


/*
 * Attribute controller.  Flip-flop reset by IGS_INPUT_STATUS1 at 0x3da.
 * We don't bother defining actual registers, we only use them once
 * during video initialization.
 */
#define IGS_ATTR_IDX		IGS_REG_(0x3c0)
#define IGS_ATTR_PORT		IGS_REG_(0x3c1)


/*
 * Misc output register.  We only use the _W register during video
 * initialization.
 */
#define IGS_MISC_OUTPUT_W	IGS_REG_(0x3c2)
#define IGS_MISC_OUTPUT_R	IGS_REG_(0x3cc)


/*
 * SEQUENCER.
 */
#define IGS_SEQ_IDX		IGS_REG_(0x3c4)
#define IGS_SEQ_PORT		IGS_REG_(0x3c5)

#define   IGS_SEQ_RESET			0x0
#define     IGS_SEQ_RESET_ASYNC			0x01
#define     IGS_SEQ_RESET_SYNC			0x02


/* IGS_EXT_SPRITE_CTL/IGS_EXT_SPRITE_DAC_PEL (3cf/56[2]) == 0 */
#define IGS_PEL_MASK		IGS_REG_(0x3c6)

/* IGS_EXT_SPRITE_CTL/IGS_EXT_SPRITE_DAC_PEL 3cf/56[2] == 1 */
#define IGS_DAC_CMD		IGS_REG_(0x3c6)


/*
 * Palette Read/Write: write palette index to the index port.
 * Read/write R/G/B in three consecutive accesses to data port.
 * After third access to data the index is autoincremented and you can
 * proceed with reading/writing data port for the next entry.
 *
 * When IGS_EXT_SPRITE_DAC_PEL bit in sprite control is set, these
 * registers are used to access sprite (i.e. cursor) 2-color palette.
 * (NB: apparently, in this mode index autoincrement doesn't work).
 */
#define IGS_DAC_PEL_READ_IDX	IGS_REG_(0x3c7)
#define IGS_DAC_PEL_WRITE_IDX	IGS_REG_(0x3c8)
#define IGS_DAC_PEL_DATA	IGS_REG_(0x3c9)


/*
 * GRAPHICS CONTROLLER registers.
 */
#define IGS_GRFX_IDX		IGS_REG_(0x3ce)
#define IGS_GRFX_PORT		IGS_REG_(0x3cf)


/*
 * EXTENDED registers.
 */
#define IGS_EXT_IDX		IGS_REG_(0x3ce)
#define IGS_EXT_PORT		IGS_REG_(0x3cf)

/* [3..0] -> [19..16] of start addr if IGS_EXT_START_ADDR_ON is set */
#define   IGS_EXT_START_ADDR		0x10
#define     IGS_EXT_START_ADDR_ON		0x10

/* overflow 10th bits for severl crtc registers; interlaced mode select */
#define   IGS_EXT_VOVFL			0x11
#define     IGS_EXT_VOVFL_INTERLACED		0x20

#define   IGS_EXT_IRQ_CTL		0x12
#define     IGS_EXT_IRQ_ENABLE			0x01



/*
 * Sync Control.
 * Two-bit combinations for h/v:
 *     00 - normal, 01 - force 0, 1x - force 1
 */
#define   IGS_EXT_SYNC_CTL		0x16
#define     IGS_EXT_SYNC_H0			0x01
#define     IGS_EXT_SYNC_H1			0x02
#define     IGS_EXT_SYNC_V0			0x04
#define     IGS_EXT_SYNC_V1			0x08

/*
 * For PCI just use normal BAR config.
 */
#define   IGS_EXT_BUS_CTL		0x30
#define     IGS_EXT_BUS_CTL_LINSIZE_SHIFT	0
#define     IGS_EXT_BUS_CTL_LINSIZE_MASK	0x03
#define     IGS_EXT_BUS_CTL_LINSIZE(x) \
    (((x) >> IGS_EXT_BUS_CTL_LINSIZE_SHIFT) & IGS_EXT_BUS_CTL_LINSIZE_MASK)

/*
 * COPREN   - enable direct access to coprocessor registers
 * COPASELB - select IGS_COP_BASE_B for COP address
 */
#define   IGS_EXT_BIU_MISC_CTL		0x33
#define     IGS_EXT_BIU_LINEAREN		0x01
#define     IGS_EXT_BIU_LIN2MEM			0x02
#define     IGS_EXT_BIU_COPREN			0x04
#define     IGS_EXT_BIU_COPASELB		0x08
#define     IGS_EXT_BIU_SEGON			0x10
#define     IGS_EXT_BIU_SEG2MEM			0x20

/*
 * Linear Address registers
 *   PCI: don't write directly, just use nomral PCI configuration
 *   ISA: only bits [23..20] are programmable, the rest MBZ
 */
#define   IGS_EXT_LINA_LO		0x34	/* [3..0] -> [23..20] */
#define   IGS_EXT_LINA_HI		0x35	/* [7..0] -> [31..24] */

/* Hardware cursor on-screen location and hot spot */
#define   IGS_EXT_SPRITE_HSTART_LO	0x50
#define   IGS_EXT_SPRITE_HSTART_HI	0x51	/* bits [2..0] */
#define   IGS_EXT_SPRITE_HPRESET	0x52	/* bits [5..0] */

#define   IGS_EXT_SPRITE_VSTART_LO	0x53
#define   IGS_EXT_SPRITE_VSTART_HI	0x54	/* bits [2..0] */
#define   IGS_EXT_SPRITE_VPRESET	0x55	/* bits [5..0] */

/* Hardware cursor control */
#define   IGS_EXT_SPRITE_CTL		0x56
#define     IGS_EXT_SPRITE_VISIBLE		0x01
#define     IGS_EXT_SPRITE_64x64		0x02
#define     IGS_EXT_SPRITE_DAC_PEL		0x04
	  /* bits unrelated to sprite control */
#define     IGS_EXT_COP_RESET			0x08

/* Extended graphics mode */
#define   IGS_EXT_GRFX_MODE		0x57
#define     IGS_EXT_GRFX_MODE_EXT		0x01

/* Overscan R/G/B registers */
#define   IGS_EXT_OVERSCAN_RED		0x58
#define   IGS_EXT_OVERSCAN_GREEN	0x59
#define   IGS_EXT_OVERSCAN_BLUE		0x5a

/* Memory controller */
#define   IGS_EXT_MEM_CTL0		0x70
#define   IGS_EXT_MEM_CTL1		0x71
#define   IGS_EXT_MEM_CTL2		0x72

/*
 * SEQ miscellaneous: number of SL between CCLK - controls visual depth.
 * These values are for MODE256 == 1, SRMODE = 1 in GRFX/5 mode register.
 */
#define   IGS_EXT_SEQ_MISC		0x77
#define     IGS_EXT_SEQ_IBM_STD			0
#define     IGS_EXT_SEQ_8BPP			1 /* 256 indexed */
#define     IGS_EXT_SEQ_16BPP			2 /* HiColor 16bpp, 5-6-5 */
#define     IGS_EXT_SEQ_32BPP			3 /* TrueColor 32bpp */
#define     IGS_EXT_SEQ_24BPP			4 /* TrueColor 24bpp */
#define     IGS_EXT_SEQ_15BPP			6 /* HiColor 16bpp, 5-5-5 */

/* Hardware cursor data location in linear memory */
#define   IGS_EXT_SPRITE_DATA_LO	0x7e
#define   IGS_EXT_SPRITE_DATA_HI	0x7f	/* bits [3..0] */


#define   IGS_EXT_VCLK0			0xb0 /* mult */
#define   IGS_EXT_VCLK1			0xb1 /*  div */
#define   IGS_EXT_MCLK0			0xb2 /* mult */
#define   IGS_EXT_MCLK1			0xb3 /*  div */


/* ----8<----  end of IGS_EXT registers  ----8<---- */



/*
 * CRTC can be at 0x3b4/0x3b5 (mono) or 0x3d4/0x3d5 (color)
 * controlled by bit 0 in misc output register (r=0x3cc/w=0x3c2).
 * We forcibly init it to color.
 */
#define IGS_CRTC_IDX		IGS_REG_(0x3d4)
#define IGS_CRTC_PORT		IGS_REG_(0x3d5)

/*
 * Reading this register resets flip-flop at 0x3c0 (attribute
 * controller) to address register.
 */
#define IGS_INPUT_STATUS1	IGS_REG_(0x3da)



/*********************************************************************
 *		       IGS Graphic Coprocessor
 */

/*
 * Coprocessor registers location in I/O space.
 * Controlled by COPASELB bit in IGS_EXT_BIU_MISC_CTL.
 */
#define IGS_COP_BASE_A	0xaf000		/* COPASELB == 0 */
#define IGS_COP_BASE_B	0xbf000		/* COPASELB == 1 */
#define IGS_COP_SIZE	0x00400


/*
 * NB: Loaded width values should be 1 less than the actual width!
 */

/*
 * Coprocessor control.
 */
#define IGS_COP_CTL_REG		0x011
#define   IGS_COP_CTL_HBRDYZ		0x01
#define   IGS_COP_CTL_HFEMPTZ		0x02
#define   IGS_COP_CTL_CMDFF		0x04
#define   IGS_COP_CTL_SOP		0x08 /* rw */
#define   IGS_COP_CTL_OPS		0x10
#define   IGS_COP_CTL_TER		0x20 /* rw */
#define   IGS_COP_CTL_HBACKZ		0x40
#define   IGS_COP_CTL_BUSY		0x80


/*
 * Source(s) and destination widths.
 * 16 bit registers.  Only bits [11..0] are used.
 */
#define IGS_COP_SRC_MAP_WIDTH_REG  0x018
#define IGS_COP_SRC2_MAP_WIDTH_REG 0x118
#define IGS_COP_DST_MAP_WIDTH_REG  0x218


/*
 * Bitmap depth.
 */
#define IGS_COP_MAP_FMT_REG	0x01c
#define   IGS_COP_MAP_8BPP		0x00
#define   IGS_COP_MAP_16BPP		0x01
#define   IGS_COP_MAP_24BPP		0x02
#define   IGS_COP_MAP_32BPP		0x03


/*
 * Binary operations are defined below.  S - source, D - destination,
 * N - not; a - and, o - or, x - xor.
 *
 * For ternary operations, foreground mix function is one of 256
 * ternary raster operations defined by Win32 API; background mix is
 * ignored.
 */
#define IGS_COP_FG_MIX_REG	0x048
#define IGS_COP_BG_MIX_REG	0x049

#define   IGS_COP_MIX_0			0x0
#define   IGS_COP_MIX_SaD		0x1
#define   IGS_COP_MIX_SaND		0x2
#define   IGS_COP_MIX_S			0x3
#define   IGS_COP_MIX_NSaD		0x4
#define   IGS_COP_MIX_D			0x5
#define   IGS_COP_MIX_SxD		0x6
#define   IGS_COP_MIX_SoD		0x7
#define   IGS_COP_MIX_NSaND		0x8
#define   IGS_COP_MIX_SxND		0x9
#define   IGS_COP_MIX_ND		0xa
#define   IGS_COP_MIX_SoND		0xb
#define   IGS_COP_MIX_NS		0xc
#define   IGS_COP_MIX_NSoD		0xd
#define   IGS_COP_MIX_NSoND		0xe
#define   IGS_COP_MIX_1			0xf


/*
 * Foreground/background colours (24 bit).
 * Selected by bits in IGS_COP_PIXEL_OP_3_REG.
 */
#define IGS_COP_FG_REG		0x058
#define IGS_COP_BG_REG		0x05C


/*
 * Horizontal/vertical dimensions of pixel blit function.
 * 16 bit registers.  Only [11..0] are used.
 */
#define IGS_COP_WIDTH_REG	0x060
#define IGS_COP_HEIGHT_REG	0x062


/*
 * Only bits [21..0] are used.
 */
#define IGS_COP_SRC_BASE_REG	0x070 /* only for 24bpp Src Color Tiling */
#define IGS_COP_SRC_START_REG	0x170
#define IGS_COP_SRC2_START_REG	0x174
#define IGS_COP_DST_START_REG	0x178

/*
 * Destination phase angle for 24bpp.
 */
#define IGS_COP_DST_X_PHASE_REG	0x078
#define   IGS_COP_DST_X_PHASE_MASK	0x07


/*
 * Pixel operation: Direction and draw mode.
 * When an octant bit is set, that axis is traversed backwards.
 */
#define IGS_COP_PIXEL_OP_0_REG	0x07c

#define   IGS_COP_OCTANT_Y_NEG		0x02 /* 0: top down, 1: bottom up */
#define   IGS_COP_OCTANT_X_NEG		0x04 /* 0: l2r, 1: r2l */

#define   IGS_COP_DRAW_ALL		0x00
#define   IGS_COP_DRAW_FIRST_NULL	0x10
#define   IGS_COP_DRAW_LAST_NULL	0x20


/*
 * Pixel operation: Pattern operation.
 */
#define IGS_COP_PIXEL_OP_1_REG	0x07d

#define   IGS_COP_PPM_TEXT		0x10
#define   IGS_COP_PPM_TILE		0x20
#define   IGS_COP_PPM_LINE		0x30
#define   IGS_COP_PPM_TRANSPARENT	0x40 /* "or" with one of the above */

#define   IGS_COP_PPM_FIXED_FG		0x80
#define   IGS_COP_PPM_SRC_COLOR_TILE	0x90


/*
 * Pixel operation: Host CPU access (host blit) to graphics engine.
 */
#define IGS_COP_PIXEL_OP_2_REG	0x07e
#define   IGS_COP_HBLTR			0x01 /* enable read from engine */
#define   IGS_COP_HBLTW			0x02 /* enable write to engine  */


/*
 * Pixel operation: Operation function of graphic engine.
 */
#define IGS_COP_PIXEL_OP_3_REG	0x07f
#define   IGS_COP_OP_STROKE		0x04 /* short stroke */
#define   IGS_COP_OP_LINE		0x05 /* bresenham line draw */
#define   IGS_COP_OP_PXBLT		0x08 /* pixel blit */
#define   IGS_COP_OP_PXBLT_INV		0x09 /* invert pixel blit */
#define   IGS_COP_OP_PXBLT_3		0x0a /* ternary pixel blit */

/* select fg/bg source: 0 - fg/bg color reg, 1 - src1 map */
#define   IGS_COP_OP_FG_FROM_SRC	0x20
#define   IGS_COP_OP_BG_FROM_SRC	0x80

#define IGS_CLOCK_REF	14318 /*24576*/	/* KHz */

#define IGS_SCALE(p)	((p) ? (2 * (p)) : 1)

#define IGS_CLOCK(m,n,p) \
	((IGS_CLOCK_REF * ((m) + 1)) / (((n) + 1) * IGS_SCALE(p)))

#define IGS_MAX_CLOCK	260000

#define IGS_MIN_VCO	115000

#endif /* _DEV_IC_IGSFBREG_H_ */
