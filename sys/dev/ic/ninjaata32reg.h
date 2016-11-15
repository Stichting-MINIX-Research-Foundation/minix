/*	$NetBSD: ninjaata32reg.h,v 1.4 2011/02/21 02:32:00 itohy Exp $	*/

/*
 * Copyright (c) 2006 ITOH Yasufumi.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NJATA32REG_H_
#define _NJATA32REG_H_

/*
 * Workbit NinjaATA (32bit versions), IDE Controller with Busmastering PIO:
 *	NinjaATA-32Bi	PCMCIA/CardBus dual mode device ("DuoATA")
 *			(CardBus mode only)
 *	NPATA-32	CardBus device
 */

/*
 * CAVEAT
 * The names and the functions of the registers are probably incorrect
 * since no programming information is available in the public.
 */

#define NJATA32_REGSIZE		32	/* size of register set */
#define NJATA32_MEMOFFSET_REG	0x860	/* offset of memory mapped register */

#define NJATA32_REG_IRQ_STAT		0x00	/* len=1 RO */
#define NJATA32_REG_IRQ_SELECT		0x01	/* len=1 WO */
# define NJATA32_IRQ_XFER		0x01
# define NJATA32_IRQ_DEV		0x04

#define NJATA32_REG_IOBM		0x02	/* len=1 WO */
# define NJATA32_IOBM_01		0x01
# define NJATA32_IOBM_02		0x02
# define NJATA32_IOBM_MMENBL		0x08
# define NJATA32_IOBM_BURST		0x10
# define NJATA32_IOBM_NO_BMSTART0	0x20
# define NJATA32_IOBM_80		0x80

# define NJATA32_IOBM_DEFAULT		(NJATA32_IOBM_01 | NJATA32_IOBM_02 | \
	NJATA32_IOBM_BURST | NJATA32_IOBM_NO_BMSTART0 | NJATA32_IOBM_80)

#define NJATA32_REG_AS			0x04	/* len=1 WO */
# define NJATA32_AS_START		0x01	/* 0: PIO BM, 1: DMA BM */
# define NJATA32_AS_WAIT0		0x00
# define NJATA32_AS_WAIT1		0x04
# define NJATA32_AS_WAIT2		0x08
# define NJATA32_AS_WAIT3		0x0c
# define NJATA32_AS_BUS_RESET		0x80

#define NJATA32_REG_DMAADDR		0x08	/* len=4 R/W */
#define NJATA32_REG_DMALENGTH		0x0c	/* len=4 R/W */

/*
 * WDC registers
 */
#define NJATA32_OFFSET_WDCREGS		0x10

#define NJATA32_REG_WD_DATA		0x10	/* len=1/2/4 R/W */
#define NJATA32_REG_WD_ERROR		0x11	/* len=1 RO */
#define NJATA32_REG_WD_FEATURES		0x11	/* len=1 WO */
#define NJATA32_REG_WD_SECCNT		0x12	/* len=1 R/W */
#define NJATA32_REG_WD_IREASON		0x12	/* len=1 R/W (ATAPI) */
#define NJATA32_REG_WD_SECTOR		0x13	/* len=1 R/W */
#define NJATA32_REG_WD_LBA_LO		0x13	/* len=1 R/W */
#define NJATA32_REG_WD_CYL_LO		0x14	/* len=1 R/W */
#define NJATA32_REG_WD_LBA_MI		0x14	/* len=1 R/W */
#define NJATA32_REG_WD_CYL_HI		0x15	/* len=1 R/W */
#define NJATA32_REG_WD_LBA_HI		0x15	/* len=1 R/W */
#define NJATA32_REG_WD_SDH		0x16	/* len=1 R/W */
#define NJATA32_REG_WD_COMMAND		0x17	/* len=1 WO */
#define NJATA32_REG_WD_STATUS		0x17	/* len=1 RO */

#if 0	/* these registers seem to show the busmaster status */
/* ? */
#define NJATA32_REG_18			0x18	/* len=4 RO */
/* ? */
#define NJATA32_REG_1c			0x1c	/* len=1 RO */
#endif

#define NJATA32_REG_BM			0x1d	/* len=1 R/W */
# define NJATA32_BM_EN			0x01
# define NJATA32_BM_RD			0x02	/* 0: write, 1: read */
# define NJATA32_BM_SG			0x04	/* 1: use scatter/gather tbl */
# define NJATA32_BM_GO			0x08
# define NJATA32_BM_WAIT0		0x00
# define NJATA32_BM_WAIT1		0x10
# define NJATA32_BM_WAIT2		0x20
# define NJATA32_BM_WAIT3		0x30
#  define NJATA32_BM_WAIT_MASK		0x30
#  define NJATA32_BM_WAIT_SHIFT		4
# define NJATA32_BM_DONE		0x80	/* ? */

#define NJATA32_REG_WD_ALTSTATUS	0x1e	/* len=1 R */

#define NJATA32_REG_TIMING		0x1f	/* len=1 W */
/* timing values for PIO transfer */
# define NJATA32_TIMING_PIO0		0xd6
# define NJATA32_TIMING_PIO1		0x85
# define NJATA32_TIMING_PIO2		0x44
# define NJATA32_TIMING_PIO3		0x33
# define NJATA32_TIMING_PIO4		0x13
# define NJATA32_TIMING_PIO4_		0x14	/* for timing tweak */
# define NJATA32_TIMING_PIO4__		0x24	/* for timing tweak */
/* timing values for multiword DMA transfer */
# define NJATA32_TIMING_DMA0		0x88
# define NJATA32_TIMING_DMA1		0x23
# define NJATA32_TIMING_DMA2		0x13
/* timing values for obsolete singleword DMA transfer */
# define NJATA32_TIMING_SMDMA0		0xff
# define NJATA32_TIMING_SMDMA1		0x88
# define NJATA32_TIMING_SMDMA2		0x44

/*
 * DMA data structure
 */

/* scatter/gather transfer table entry (8 bytes) */
struct njata32_sgtable {
	uint32_t	sg_addr;	/* transfer address (little endian) */
	uint32_t	sg_len;		/* transfer length (little endian) */
#define NJATA32_SGT_ENDMARK	0x80000000
#define NJATA32_SGT_MAXSEGLEN	0x10000
};
#define NJATA32_SGT_MAXENTRY	18

/*
 * device specific constants
 */
#define NJATA32_MODE_MAX_DMA	2
#define NJATA32_MODE_MAX_PIO	4

#endif	/* _NJATA32REG_H_ */
