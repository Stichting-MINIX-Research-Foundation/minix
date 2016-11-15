/*	$NetBSD: tdvfbreg.h,v 1.3 2012/07/20 21:31:28 rkujawa Exp $	*/

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

/* 3Dfx Voodoo 2 register definition (mostly from specification) */

#ifndef TDVFBREG_H
#define TDVFBREG_H

#define TDV_SST_CLK		50000	/* 50MHz, max is around 60MHz */
#define TDV_CVG_CLK		75000	/* 75MHz, max is around 90MHz */

/* CVG PCI config registers */
#define TDV_MM_BAR		0x10

#define TDV_INITENABLE_REG	0x40
#define TDV_INITENABLE_EN_INIT  __BIT(0)	
#define TDV_INITENABLE_EN_FIFO	__BIT(1)	
#define TDV_INITENABLE_REMAPDAC __BIT(2)
#define TDV_VCLK_ENABLE_REG	0xC0	/* undocumented? */
#define TDV_VCLK_DISABLE_REG	0xE0	/* undocumented? */

/* CVG address space */
#define TDV_OFF_MMREG		0x0	/* memory mapped registers */
#define TDV_OFF_FB		0x400000/* frame buffer */
#define TDV_FB_SIZE		0x3FFFFF/* 4MB */

/* CVG registers */
#define TDV_OFF_STATUS		0x0
#define TDV_STATUS_FBI_BUSY	__BIT(7) /* FBI == CVG Bruce */
#define TDV_STATUS_TREX_BUSY	__BIT(8) /* TREX == CVG Chuck */
#define TDV_STATUS_GFX_BUSY	__BIT(9)

#define TDV_OFF_ALPHAMODE	0x10C

#define TDV_OFF_FBZMODE		0x0110
#define TDV_FBZMODE_CLIPPING	__BIT(0)	
#define TDV_FBZMODE_RGB_WR	__BIT(9)
#define TDV_FBZMODE_ALPHA_WR	__BIT(10)
#define TDV_FBZMODE_INVERT_Y	__BIT(17)

#define TDV_OFF_LFBMODE		0x0114
#define TDV_LFBMODE_565		0 //__BIT(0)	
#define TDV_LFBMODE_8888	5 
#define TDV_LFBMODE_PIXPIPE	__BIT(8)
#define TDV_LFBMODE_WSW_WR	__BIT(11)
#define TDV_LFBMODE_BSW_WR	__BIT(12)
#define TDV_LFBMODE_WSW_RD	__BIT(15)
#define TDV_LFBMODE_BSW_RD	__BIT(16) 

#define TDV_OFF_CLIP_LR		0x0118
#define TDV_OFF_CLIP_TB		0x011C

#define TDV_OFF_NOPCMD		0x0120

#define TDV_OFF_FBIINIT0	0x0210
#define TDV_FBIINIT0_VGA_PASS	__BIT(0)
#define TDV_FBIINIT0_FBI_RST	__BIT(1)
#define TDV_FBIINIT0_FIFO_RST	__BIT(2)

#define TDV_OFF_FBIINIT1	0x0214
#define TDV_FBIINIT1_PCIWAIT	__BIT(1)
#define TDV_FBIINIT1_LFB_EN	__BIT(3)
#define TDV_FBIINIT1_TILES_X	4	/* shift, bits 4-7 */
#define TDV_FBIINIT1_VIDEO_RST	__BIT(8)
#define TDV_FBIINIT1_BLANKING	__BIT(12)
#define TDV_FBIINIT1_DR_DATA	__BIT(13)
#define TDV_FBIINIT1_DR_BLANKING __BIT(14)
#define TDV_FBIINIT1_DR_HVSYNC	__BIT(15)
#define TDV_FBIINIT1_DR_DCLK	__BIT(16)
#define TDV_FBIINIT1_IN_VCLK_2X 0	/* __BIT(17) */
#define TDV_FBIINIT1_VCLK_SRC	20	/* shift, bits 20-21 actually */
#define TDV_FBIINIT1_VCLK_2X	0x2
#define TDV_FBIINIT1_TILES_X_MSB 24
#define TDV_FBIINIT1_VIDMASK	0x8080010F

#define TDV_OFF_FBIINIT2	0x0218
#define TDV_OFF_DAC_READ	TDV_OFF_FBIINIT2
#define TDV_FBIINIT2_FAST_RAS	__BIT(5)
#define TDV_FBIINIT2_DRAM_OE	__BIT(6)
#define TDV_FBIINIT2_SWB_ALG	0 //__BITS(9,10) /* 00 - based on DAC vsync */
#define TDV_FBIINIT2_FIFO_RDA	__BIT(21)
#define TDV_FBIINIT2_DRAM_REFR	__BIT(22)
#define TDV_FBIINIT2_DRAM_REFLD	23	/* shift, bits 23-31 */
#define TDV_FBIINIT2_DRAM_REF16 0x30	/* 16ms */

#define TDV_OFF_FBIINIT3	0x021C
#define TDV_FBIINIT3_TREX_DIS	__BIT(6)

#define TDV_OFF_FBIINIT4	0x0200
#define TDV_FBIINIT4_PCIWAIT	__BIT(0)
#define TDV_FBIINIT4_LFB_RDA	__BIT(1)

#define TDV_OFF_BACKPORCH	0x0208
#define TDV_OFF_VDIMENSIONS	0x020C
#define TDV_OFF_HSYNC		0x0220
#define TDV_OFF_VSYNC		0x0224

#define TDV_OFF_DAC_DATA	0x022C
#define TDV_DAC_DATA_READ	__BIT(11)

#define TDV_OFF_FBIINIT5	0x0244
#define TDV_FBIINIT5_VIDMASK	0xFA40FFFF
#define TDV_FBIINIT5_PHSYNC	__BIT(23)	
#define TDV_FBIINIT5_PVSYNC	__BIT(24)

#define TDV_OFF_FBIINIT6	0x0248
#define TDV_FBIINIT6_TILES_X_LSB 30

#define TDV_OFF_BLTSRC		0x02C0
#define TDV_OFF_BLTDST		0x02C4
#define TDV_OFF_BLTXYSTRIDE	0x02C8
#define TDV_OFF_BLTSRCCHROMA	0x02CC
#define TDV_OFF_BLTDSTCHROMA	0x02D0
#define TDV_OFF_BLTCLIPX	0x02D4
#define TDV_OFF_BLTCLIPY	0x02D8
#define TDV_OFF_BLTSRCXY	0x02E0
#define TDV_OFF_BLTDSTXY	0x02E4
#define TDV_OFF_BLTSIZE		0x02E8
#define TDV_OFF_BLTROP		0x02EC
#define TDV_BLTROP_COPY		0x0CCCC
#define TDV_BLTROP_INVERT	0x05555
#define TDV_BLTROP_XOR		0x06666
#define TDV_OFF_BLTCOLOR	0x02F0
#define TDV_OFF_BLTCMD		0x02F8
#define TDV_BLTCMD_SCR2SCR	0
#define TDV_BLTCMD_CPU2SCR	1
#define TDV_BLTCMD_RECTFILL	2
#define TDV_BLTCMD_LAUNCH	__BIT(31)
#define TDV_BLTCMD_FMT_565	2
#define TDV_BLTCMD_CLIPRECT	__BIT(16)
#define TDV_BLTCMD_DSTTILED	__BIT(15)
#define TDV_OFF_DATA		0x02FC /* CPU2SCR */

/* DAC */
#define TDV_GENDAC_REFFREQ	14318
#define TDV_GENDAC_MAXVCO	250000	/* not sure about that */

#define TDV_GENDAC_MIN_N1	1
#define TDV_GENDAC_MAX_N1	31
#define TDV_GENDAC_MIN_N2	0
#define TDV_GENDAC_MAX_N2	3
#define TDV_GENDAC_MIN_M	1
#define TDV_GENDAC_MAX_M	127	

#define TDV_GENDAC_ADDRMASK	0x07

#define TDV_GENDAC_WR		0x0
#define TDV_GENDAC_LUT		0x01
#define TDV_GENDAC_PIXMASK	0x02
#define TDV_GENDAC_RD		0x03

#define TDV_GENDAC_PLLWR	0x04
#define TDV_GENDAC_PLLDATA	0x05
#define TDV_GENDAC_CMD		0x06
#define TDV_GENDAC_CMD_16BITS	0x50
#define TDV_GENDAC_CMD_24BITS	0x60
#define TDV_GENDAC_CMD_PWDOWN	__BIT(0)
#define TDV_GENDAC_PLLRD	0x07

#define TDV_GENDAC_PLL_A	0xA
#define TDV_GENDAC_PLL_0	0x0

#define TDV_GENDAC_PLL_CTRL	0x0
#define TDV_GENDAC_PLL_VIDCLK	__BIT(5)
#define TDV_GENDAC_PLL_VIDCLK0	0	
#define TDV_GENDAC_PLL_CVGCLKA	0

#define TDV_GENDAC_CVGPLLMASK	0xEF
#define TDV_GENDAC_VIDPLLMASK	0xD8

#define TDV_GENDAC_DFLT_F1_M	0x55
#define TDV_GENDAC_DFLT_F1_N	0x49
#define TDV_GENDAC_DFLT_F7_M	0x71
#define TDV_GENDAC_DFLT_F7_N	0x29
#define TDV_GENDAC_DFLT_FB_M	0x79
#define TDV_GENDAC_DFLT_FB_N	0x2E

#endif /* TDVFBREG_H */
