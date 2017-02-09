/*	$NetBSD: cgtwelvereg.h,v 1.2 2010/04/14 04:37:11 macallan Exp $ */

/*-
 * Copyright (c) 2010 Michael Lorenz
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

/* 
 * some hardware constants for the CG12 / Matrox SG3
 * mostly from SMI's cg12reg.h
 */

#ifndef CG12REG_H
#define CG12REG_H

/* SBus offsets known so far */
#define CG12_FB_MONO		0x780000

#define	CG12_OFF_PROM		0x000000
#define	CG12_OFF_USSC		0x040000
#define CG12_OFF_REGISTERS	0x040000
#define	CG12_OFF_DPU		0x040100
#define	CG12_OFF_APU		0x040200
#define	CG12_OFF_DAC		0x040300
#define	CG12_OFF_DAC_ADDR0	0x040300
#define	CG12_OFF_DAC_ADDR1	0x040400
#define	CG12_OFF_DAC_CTRL	0x040500
#define	CG12_OFF_DAC_PRIME	0x040600
#define	CG12_OFF_EIC		0x040700
#define	CG12_OFF_WSC		0x040800
#define	CG12_OFF_WSC_DATA	0x040800
#define	CG12_OFF_WSC_ADDR	0x040900
#define	CG12_OFF_DRAM		0x400000
#define	CG12_OFF_SHMEM		CG12_OFF_DRAM + 0x0E0000
#define	CG12_OFF_DISPLAY	0x600000
#define	CG12_OFF_WID		0x600000
#define	CG12_OFF_OVERLAY0	0x700000
#define	CG12_OFF_OVERLAY1	0x780000
#define	CG12_OFF_INTEN		0x800000
#define	CG12_OFF_DEPTH		0xC00000

#define	CG12_OFF_CTL		CG12_OFF_USSC	/* 0x040000 */

#define	CG12_PROM_SIZE		0x010000
#define	CG12_USSC_SIZE		0x000060	/* ### check up */
#define	CG12_DPU_SIZE		0x000080
#define	CG12_APU_SIZE		0x000100
#define	CG12_DAC_SIZE		0x000400
#define	CG12_EIC_SIZE		0x000040
#define	CG12_WSC_SIZE		0x000200
#define	CG12_WSC_ADDR_SIZE	0x000100
#define	CG12_WSC_DATA_SIZE	0x000100
#define	CG12_DRAM_SIZE		0x100000
#define	CG12_COLOR24_SIZE	0x400000
#define	CG12_COLOR8_SIZE	0x100000
#define	CG12_ZBUF_SIZE		0x200000
#define	CG12_WID_SIZE		0x100000
#define	CG12_OVERLAY_SIZE	0x020000
#define	CG12_ENABLE_SIZE	0x020000

#define	CG12_SHMEM_SIZE		0x020000
#define	CG12_FBCTL_SIZE		0x842000
#define	CG12_PMCTL_SIZE		0x041000

/* DPU registers, all register offsets are relative to CG12_OFF_REGISTERS */
#define CG12DPU_R0		0x0100	
#define CG12DPU_R1		0x0104
#define CG12DPU_R2		0x0108
#define CG12DPU_R3		0x010c
#define CG12DPU_R4		0x0110
#define CG12DPU_R5		0x0114
#define CG12DPU_R6		0x0118
#define CG12DPU_R7		0x011c
#define CG12DPU_RELOAD_CTL	0x0120
#define CG12DPU_RELOAD_STB	0x0124
#define CG12DPU_ALU_CTL		0x0128
#define CG12DPU_BLU_CTL		0x012c
#define CG12DPU_CONTROL		0x0130
#define CG12DPU_XLEFT		0x0134
#define CG12DPU_SHIFT_0		0x0138
#define CG12DPU_SHIFT_1		0x013c
#define CG12DPU_ZOOM		0x0140
#define CG12DPU_BSR		0x0144
#define CG12DPU_COLOUR0		0x0148
#define CG12DPU_COLOUR1		0x014c
#define CG12DPU_COMP_OUT	0x0150
#define CG12DPU_PLN_RDMSK_HOST	0x0154
#define CG12DPU_PLN_WRMSK_HOST	0x0158
#define CG12DPU_PLN_RDMSK_LOC	0x015c
#define CG12DPU_PLN_WRMSK_LOC	0x0160
#define CG12DPU_SCIS_CTL	0x0164
#define CG12DPU_CSR		0x0168
#define CG12DPU_PLN_REG_SL	0x016c
#define CG12DPU_PLN_SL_HOST	0x0170
#define CG12DPU_PLN_SL_LOCAL0	0x0174
#define CG12DPU_PLN_SL_LOCAL1	0x0178
#define CG12DPU_BROADCAST	0x017c

/* APU registers */
#define CG12APU_IMSG0		0x0200
#define CG12APU_MSG0		0x0204
#define CG12APU_IMSG1		0x0208
#define CG12APU_MSG1		0x020c
#define CG12APU_IEN0		0x0210
#define CG12APU_IEN1		0x0214
#define CG12APU_ICLEAR		0x0218
#define CG12APU_ISTATUS		0x021c
#define CG12APU_CFCNT		0x0220
#define CG12APU_CFWPTR		0x0224
#define CG12APU_CFRPTR		0x0228
#define CG12APU_CFILEV0		0x022c
#define CG12APU_CFILEV1		0x0230
#define CG12APU_RFCNT		0x0234
#define CG12APU_RFWPTR		0x0238
#define CG12APU_RFRPTR		0x023c
#define CG12APU_RFILEV0		0x0240
#define CG12APU_RFILEV1		0x0244
#define CG12APU_SIZE		0x0248
#define CG12APU_RES0		0x024c
#define CG12APU_RES1		0x0250
#define CG12APU_RES2		0x0254
#define CG12APU_HACCESS		0x0258
#define CG12APU_HPAGE		0x025c
#define CG12APU_LACCESS		0x0260
#define CG12APU_LPAGE		0x0264
#define CG12APU_MACCESS		0x0268
#define CG12APU_PPAGE		0x026c
#define CG12APU_DWG_CTL		0x0270
/* 
 * The following bits are from Matrox Athena docs, they're probably not all
 * implemented or not in the same spot on the cg12. They're here strictly
 * for testing.
 */
#define		DWGCTL_LINE_OPEN	0x00000000
#define		DWGCTL_AUTOLINE_OPEN	0x00000001
#define		DWGCTL_LINE_CLOSED	0x00000002
#define		DWGCTL_AUTOLINE_CLOSED	0x00000003
#define		DWGCTL_TRAPEXOID	0x00000004
#define		DWGCTL_BITBLT		0x00000008
#define		DWGCTL_UPLOAD		0x00000009
#define		DWGCTL_DOWNLOAD		0x0000000a
#define		DWGCTL_WRITE		0x00000000	/* write only */
#define		DWGCTL_RASTER		0x00000010	/* read/write */
#define		DWGCTL_ANTIALIAS	0x00000020
#define		DWGCTL_BLOCKMODE	0x00000040
#define		DWGCTL_LINEAR		0x00000080	/* XY otherwise */
#define		DWGCTL_ROP_MASK		0x000f0000
#define		DWGCTL_ROP_SHIFT	16
#define		DWGCTL_TRANSLUCID_MASK	0x00f00000
#define		DWGCTL_TRANSLUCID_SHIFT	20		/* selects pattern */
#define		DWGCTL_BLTMOD_MONO	0x00000000
#define		DWGCTL_BLTMOD_PLANE	0x02000000
#define		DWGCTL_BLTMOD_COLOR	0x04000000	/* clipping usable */
#define		DWGCTL_BLTMOD_UCOLOR	0x06000000	/* no clipping */
#define		DWGCTL_AFOR		0x08000000	/* set for antialias */
#define		DWGCTL_UPLOAD_RGB	0x08000000	/* BGR otherwise */
#define		DWGCTL_AA_BG		0x10000000	/* us BG color in AA */
#define		DWGCTL_UPLOAD_24BIT	0x10000000	/* 32bit otherwise */
#define		DWGCTL_EN_PATTERN	0x20000000
#define		DWGCTL_BLT_TRANSPARENT	0x40000000	/* for color exp. */

#define CG12APU_SAM		0x0274
#define CG12APU_SGN		0x0278
#define CG12APU_LENGTH		0x027c
#define CG12APU_DWG_R0		0x0280
#define CG12APU_DWG_R1		0x0284
#define CG12APU_DWG_R2		0x0288
#define CG12APU_DWG_R3		0x028c
#define CG12APU_DWG_R4		0x0290
#define CG12APU_DWG_R5		0x0294
#define CG12APU_DWG_R6		0x0298
#define CG12APU_DWG_R7		0x029c
#define CG12APU_RELOAD_CTL	0x02a0
#define CG12APU_RELOAD_STB	0x02a4
#define CG12APU_C_XLEFT		0x02a8
#define CG12APU_C_YTOP		0x02ac
#define CG12APU_C_XRIGHT	0x02b0
#define CG12APU_C_YBOTTOM	0x02b4
#define CG12APU_F_XLEFT		0x02b8
#define CG12APU_F_XRIGHT	0x02bc
#define CG12APU_X_DST		0x02c0
#define CG12APU_Y_DST		0x02c4
#define CG12APU_DST_CTL		0x02c8
#define CG12APU_MORIGIN		0x02cc
#define CG12APU_VSG_CTL		0x02d0
#define CG12APU_H_SYNC		0x02d4
#define CG12APU_H_BLANK		0x02d8
#define CG12APU_V_SYNC		0x02dc
#define CG12APU_V_BLANK		0x02e0
#define CG12APU_VDPYINT		0x02e4
#define CG12APU_VSSYNCS		0x02e8
#define CG12APU_H_DELAYS	0x02ec
#define CG12APU_STDADDR		0x02f0
#define CG12APU_HPITCHES	0x02f4
#define CG12APU_ZOOM		0x02f8
#define CG12APU_TEST		0x02fc

/*
 * The "direct port access" register constants.
 * All HACCESSS values include noHSTXY, noHCLIP, and SWAP.
 */

#define	CG12_HPAGE_OVERLAY	0x00000700	/* overlay page		*/
#define	CG12_HACCESS_OVERLAY	0x00000020	/* 1bit/pixel		*/
#define	CG12_PLN_SL_OVERLAY	0x00000017	/* plane 23		*/
#define	CG12_PLN_WR_OVERLAY	0x00800000	/* write mask		*/
#define	CG12_PLN_RD_OVERLAY	0xffffffff	/* read mask		*/

#define	CG12_HPAGE_ENABLE	0x00000700	/* overlay page		*/
#define	CG12_HACCESS_ENABLE	0x00000020	/* 1bit/pixel		*/
#define	CG12_PLN_SL_ENABLE	0x00000016	/* plane 22		*/
#define	CG12_PLN_WR_ENABLE	0x00400000
#define	CG12_PLN_RD_ENABLE	0xffffffff

#define	CG12_HPAGE_24BIT	0x00000500	/* intensity page	*/
#define	CG12_HACCESS_24BIT	0x00000025	/* 32bits/pixel		*/
#define	CG12_PLN_SL_24BIT	0x00000000	/* all planes		*/
#define	CG12_PLN_WR_24BIT	0x00ffffff
#define	CG12_PLN_RD_24BIT	0x00ffffff

#define	CG12_HPAGE_8BIT		0x00000500	/* intensity page	*/
#define	CG12_HACCESS_8BIT	0x00000023	/* 8bits/pixel		*/
#define	CG12_PLN_SL_8BIT	0x00000000
#define	CG12_PLN_WR_8BIT	0x00ffffff
#define	CG12_PLN_RD_8BIT	0x000000ff

#define	CG12_HPAGE_WID		0x00000700	/* overlay page		*/
#define	CG12_HACCESS_WID	0x00000023	/* 8bits/pixel		*/
#define	CG12_PLN_SL_WID		0x00000010	/* planes 16-23		*/
#define	CG12_PLN_WR_WID		0x003f0000
#define	CG12_PLN_RD_WID		0x003f0000

#define	CG12_HPAGE_ZBUF		0x00000000	/* depth page		*/
#define	CG12_HACCESS_ZBUF	0x00000024	/* 16bits/pixel		*/
#define	CG12_PLN_SL_ZBUF	0x00000060
#define	CG12_PLN_WR_ZBUF	0xffffffff
#define	CG12_PLN_RD_ZBUF	0xffffffff

/* RAMDAC registers */
#define CG12DAC_ADDR0		0x0300
#define CG12DAC_ADDR1		0x0400
#define CG12DAC_CTRL		0x0500
#define CG12DAC_DATA		0x0600

/* WIDs */
#define	CG12_WID_8_BIT		0	/* indexed color		*/
#define	CG12_WID_24_BIT		1	/* true color			*/
#define	CG12_WID_ENABLE_2	2	/* overlay/cursor enable has 2 colors */
#define	CG12_WID_ENABLE_3	3	/* overlay/cursor enable has 3 colors */
#define	CG12_WID_ALT_CMAP	4	/* use alternate colormap	*/
#define	CG12_WID_DBL_BUF_DISP_A	5	/* double buffering display A	*/
#define	CG12_WID_DBL_BUF_DISP_B	6	/* double buffering display A	*/
#define	CG12_WID_ATTRS		7	/* total no of attributes	*/

/* WSC */
#define	CG12_WSC_DATA		0x0800
#define	CG12_WSC_ADDR		0x0900

/* EIC registers */
#define CG12_EIC_HOST_CONTROL	0x0700
#define CG12_EIC_CONTROL	0x0704
#define CG12_EIC_C30_CONTROL	0x0708
#define CG12_EIC_INTERRUPT	0x070c
#define CG12_EIC_DCADDRW	0x0710
#define CG12_EIC_DCBYTEW	0x0714
#define CG12_EIC_DCSHORTW	0x0718
#define CG12_EIC_DCLONGW	0x071c
#define CG12_EIC_DCFLOATW	0x0720
#define CG12_EIC_DCADDRR	0x0724
#define CG12_EIC_DCBYTER	0x0728
#define CG12_EIC_DCSHORTR	0x072c
#define CG12_EIC_DCLONGR	0x0730
#define CG12_EIC_DCFLOATR	0x0734
#define CG12_EIC_RESET		0x073c

#endif /* CG12REG_H */
