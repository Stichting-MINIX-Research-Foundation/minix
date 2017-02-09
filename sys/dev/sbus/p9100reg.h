/*	$NetBSD: p9100reg.h,v 1.6 2012/09/21 01:07:44 macallan Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
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


#ifndef P9100_REG_H
#define P9100_REG_H

/* The Tadpole 3GX Technical Reference Manual lies.  The ramdac registers
 * are map in 4 byte increments, not 8.
 */
#define	SCRN_RPNT_CTL_1	0x0138	/* Screen Respaint Timing Control 1 */
#define	VIDEO_ENABLED	0x00000020
#define	PWRUP_CNFG	0x0194	/* Power Up Configuration */
#define	DAC_CMAP_WRIDX	0x0200	/* IBM RGB528 Palette Address (Write) */
#define	DAC_CMAP_DATA	0x0204	/* IBM RGB528 Palette Data */
#define	DAC_PXL_MASK	0x0208	/* IBM RGB528 Pixel Mask */
#define	DAC_CMAP_RDIDX	0x020c	/* IBM RGB528 Palette Address (Read) */
#define	DAC_INDX_LO	0x0210	/* IBM RGB528 Index Low */
#define	DAC_INDX_HI	0x0214	/* IBM RGB528 Index High */
#define	DAC_INDX_DATA	0x0218	/* IBM RGB528 Index Data (Indexed Registers) */
#define	DAC_INDX_CTL	0x021c	/* IBM RGB528 Index Control */
	#define DAC_INDX_AUTOINCR	0x01
	
#define DAC_VERSION	0x01
#define DAC_MISC_CLK    0x02
#define DAC_POWER_MGT	0x05
	#define DAC_POWER_SCLK_DISABLE	0x10
	#define DAC_POWER_DDOT_DISABLE	0x08
	#define DAC_POWER_SYNC_DISABLE	0x04
	/* Disable internal DAC clock */
	#define DAC_POWER_ICLK_DISABLE	0x02
	/* Disable internal DAC power */
	#define DAC_POWER_IPWR_DISABLE	0x01        
#define DAC_OPERATION   0x06
	#define DAC_SYNC_ON_GREEN       0x08
#define DAC_PALETTE_CTRL 0x07
#define DAC_PIXEL_FMT   0x0a
#define DAC_8BIT_CTRL   0x0b
	#define DAC8_DIRECT_COLOR       0x01
#define DAC_16BIT_CTRL  0x0c
	#define DAC16_INDIRECT_COLOR    0x00
	#define DAC16_DYNAMIC_COLOR     0x40
	#define DAC16_DIRECT_COLOR      0xc0
	#define DAC16_BYPASS_POLARITY   0x20
	#define DAC16_BIT_FILL_LINEAR   0x04
	#define DAC16_555               0x00
	#define DAC16_565               0x02
	#define DAC16_CONTIGUOUS        0x01
#define DAC_24BIT_CTRL  0x0d
	#define DAC24_DIRECT_COLOR      0x01
#define DAC_32BIT_CTRL  0x0e
	#define DAC32_BYPASS_POLARITY   0x04
	#define DAC32_INDIRECT_COLOR    0x00
	#define DAC32_DYNAMIC_COLOR     0x01
	#define DAC32_DIRECT_COLOR      0x03
#define DAC_VCO_DIV     0x16
#define DAC_PLL0        0x20
#define DAC_MISC_1      0x70
#define DAC_MISC_2      0x71
#define DAC_MISC_3      0x72
                                                                                                                      
#define DAC_CURSOR_CTL	0x30
	#define DAC_CURSOR_OFF	0x00
	#define DAC_CURSOR_WIN	0x02
	#define DAC_CURSOR_X11	0x03
	#define DAC_CURSOR_64	0x04	/* clear for 32x32 cursor */
#define DAC_CURSOR_X		0x31	/* 8-low, 8-high */
#define DAC_CURSOR_Y		0x33	/* 8-low, 8-high */
#define DAC_CURSOR_HOT_X	0x35	/* hotspot */
#define DAC_CURSOR_HOT_Y	0x36
#define DAC_CURSOR_COL_1	0x40	/* red. green and blue */
#define DAC_CURSOR_COL_2	0x43	
#define DAC_CURSOR_COL_3	0x46	
#define DAC_PIX_PLL		0x8e
#define DAC_CURSOR_DATA		0x100

/* main registers */
#define SYS_CONF        0x0004  /* System Configuration Register */
        #define BUFFER_WRITE_1  0x0200  /* writes got o buffer 1 */
        #define BUFFER_WRITE_0  0x0000  /* writes go to buffer 0 */
        #define BUFFER_READ_1   0x0400  /* read from buffer 1 */
        #define BUFFER_READ_0   0x0000  
        #define MEM_SWAP_BITS   0x0800  /* swap bits when accessing VRAM */
        #define MEM_SWAP_BYTES  0x1000  /* swap bytes when accessing VRAM */
        #define MEM_SWAP_HWORDS 0x2000  /* swap halfwords when accessing VRAM */
        #define SHIFT_0         14      
        #define SHIFT_1         17
        #define SHIFT_2         20
        #define SHIFT_3         29
        #define PIXEL_SHIFT     26
        #define SWAP_SHIFT      11
        /* this is what the 3GX manual says */
        #define SC_8BIT         2
        #define SC_16BIT        3
        #define SC_24BIT        7
        #define SC_32BIT        5

/* video controller registers */
#define VID_HCOUNTER    0x104
#define VID_HTOTAL      0x108
#define VID_HSRE        0x10c   /* hsync raising edge */
#define VID_HBRE        0x110   /* hblank raising edge */
#define VID_HBFE        0x114   /* hblank falling edge */
#define VID_HCNTPRLD    0x118   /* hcounter preload */
#define VID_VCOUNTER    0x11c   /* vcounter */
#define VID_VLENGTH     0x120   /* lines, including blanks */
#define VID_VSRE        0x124   /* vsync raising edge */
#define VID_VBRE        0x128   /* vblank raising edge */
#define VID_VBFE        0x12c   /* vblank falling edge */
#define VID_VCNTPRLD    0x130   /* vcounter preload */
#define VID_SRADDR      0x134   /* screen repaint address */
#define VID_SRTC        0x138   /* screen repaint timing control */
#define VID_QSFCNTR     0x13c   /* QSF counter */

#define VID_MEM_CONFIG  0x184   /* memory config */
#define VID_RFPERIOD    0x188   /* refresh period */
#define VID_RFCOUNT     0x18c   /* refresh counter */
#define VID_RLMAX       0x190   /* RAS low max */
#define VID_RLCUR       0x194   /* RAS low current */
#define VID_DACSYNC     0x198   /* read after last DAC access */

#define ENGINE_STATUS	0x2000	/* drawing engine status register */
	#define BLITTER_BUSY	0x80000000
	#define ENGINE_BUSY	0x40000000
#define COMMAND_BLIT		0x2004
#define COMMAND_QUAD		0x2008
#define COMMAND_PIXEL8		0x200c
/* pixel data for monochrome colour expansion */
#define PIXEL_1			0x2080	
/* apparently bits 2-6 control how many pixels we write - n+1 */

/* drawing engine registers */
#define COORD_INDEX		0x218c
#define WINDOW_OFFSET		0x2190

#define FOREGROUND_COLOR	0x2200
#define BACKGROUND_COLOR	0x2204
#define PLANE_MASK			0x2208
#define DRAW_MODE			0x220c	
#define PATTERN_ORIGIN_X	0x2210
#define PATTERN_ORIGIN_Y	0x2214
#define RASTER_OP			0x2218
	#define ROP_NO_SOLID		0x02000	/* if set use pattern instead of color for quad operations */
	#define ROP_2BIT_PATTERN	0x04000 /* 4-colour pattern instead of mono */
	#define ROP_PIX1_TRANS		0x08000	/* transparent background in mono */
	#define ROP_OVERSIZE		0x10000
	#define ROP_PATTERN		0x20000		/* the manual says pattern enable */
	#define ROP_TRANS		0x20000		/* but XFree86 says trans */
	#define ROP_SRC 		0xCC
	#define ROP_PAT			0xF0
	#define ROP_DST 		0xAA
	#define ROP_SET			0xff

#define PIXEL_8				0x221c
#define WINDOW_MIN			0x2220
#define WINDOW_MAX			0x2224

#define PATTERN0			0x2280
#define PATTERN1			0x2284
#define PATTERN2			0x2288
#define PATTERN3			0x228c
#define USER0				0x2290
#define USER1				0x2294
#define USER2				0x2298
#define USER3				0x229c
#define BYTE_CLIP_MIN		0x22a0
#define BYTE_CLIP_MAX		0x22a4

/* coordinate registers */
#define ABS_X0		0x3008
#define ABS_Y0		0x3010
#define ABS_XY0		0x3018
#define REL_X0		0x3028
#define REL_Y0		0x3030
#define REL_XY0		0x3038

#define ABS_X1		0x3048
#define ABS_Y1		0x3050
#define ABS_XY1		0x3058
#define REL_X1		0x3068
#define REL_Y1		0x3070
#define REL_XY1		0x3078

#define ABS_X2		0x3088
#define ABS_Y2		0x3090
#define ABS_XY2		0x3098
#define REL_X2		0x30a8
#define REL_Y2		0x30b0
#define REL_XY2		0x30b8

#define ABS_X3		0x30c8
#define ABS_Y3		0x30d0
#define ABS_XY3		0x30d8
#define REL_X3		0x30e8
#define REL_Y3		0x30f0
#define REL_XY3		0x30f8

/* meta-coordinates */
#define POINT_RTW_X		0x3208
#define POINT_RTW_Y		0x3210
#define POINT_RTW_XY	0x3218
#define POINT_RTP_X		0x3228
#define POINT_RTP_Y		0x3220
#define POINT_RTP_XY	0x3238

#define LINE_RTW_X		0x3248
#define LINE_RTW_Y		0x3250
#define LINE_RTW_XY		0x3258
#define LINE_RTP_X		0x3268
#define LINE_RTP_Y		0x3260
#define LINE_RTP_XY		0x3278

#define TRIANGLE_RTW_X	0x3288
#define TRIANGLE_RTW_Y	0x3290
#define TRIANGLE_RTW_XY	0x3298
#define TRIANGLE_RTP_X	0x32a8
#define TRIANGLE_RTP_Y	0x32a0
#define TRIANGLE_RTP_XY	0x32b8

#define QUAD_RTW_X		0x32c8
#define QUAD_RTW_Y		0x32d0
#define QUAD_RTW_XY		0x32d8
#define QUAD_RTP_X		0x32e8
#define QUAD_RTP_Y		0x32e0
#define QUAD_RTP_XY		0x32f8

#define RECT_RTW_X		0x3308
#define RECT_RTW_Y		0x3310
#define RECT_RTW_XY		0x3318
#define RECT_RTP_X		0x3328
#define RECT_RTP_Y		0x3320
#define RECT_RTP_XY		0x3338

#endif
