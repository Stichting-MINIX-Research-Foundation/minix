/*	$NetBSD: gtsdmareg.h,v 1.5 2010/04/28 13:51:56 kiyohara Exp $	*/

/*
 * Copyright (c) 2002 Allegro Networks, Inc., Wasabi Systems, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Allegro Networks, Inc., and Wasabi Systems, Inc.
 * 4. The name of Allegro Networks, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 * 5. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ALLEGRO NETWORKS, INC. AND
 * WASABI SYSTEMS, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL EITHER ALLEGRO NETWORKS, INC. OR WASABI SYSTEMS, INC.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * gtsdmareg.h - register defines for GT-64260 SDMA
 *
 * creation	Sun Apr  8 20:22:51 PDT 2001	cliff
 */

#ifndef _GTSDMAREG_H
#define _GTSDMAREG_H

#ifndef BIT
#define BIT(bitno)          (1U << (bitno))
#endif
#ifndef BITS
#define BITS(hi, lo)        ((~((~0) << ((hi) + 1))) & ((~0) << (lo)))
#endif

#define GTSDMA_BASE(u)	((u) == 0 ? 0x4000 : 0x6000)
#define GTSDMA_SIZE	0x1000

/*******************************************************************************
 *
 * SDMA register address offsets relative to the base mapping
 */
#define SDMA_SDC	0x000		/* SDMA Configuration Register */
#define SDMA_SDCM	0x008		/* SDMA Command Register */
#define SDMA_SCRDP	0x810		/* SDMA Current RX Desc. Pointer */
#define SDMA_SCTDP	0xc10		/* SDMA Current TX Desc. Pointer */
#define SDMA_SFTDP	0xc14		/* SDMA First   TX Desc. Pointer */

#define SDMA_ICAUSE	0xb800		/* Interrupt Cause Register */
#define SDMA_IMASK	0xb880		/* Interrupt Mask Register */


/*******************************************************************************
 *
 * SDMA register values and bit definitions
 */
/*
 * SDMA Configuration Register
 */
#define SDMA_SDC_RFT		BIT(0)		/* RX FIFO Threshold */
#define SDMA_SDC_SFM		BIT(1)		/* Single Frame Mode */
#define SDMA_SDC_RC_MASK	BITS(5,2)	/* Re-TX  count */
#define SDMA_SDC_RC_SHIFT	2
#define SDMA_SDC_BLMR		BIT(6)		/* RX Big=0 Lil=1 Endian mode */
#define SDMA_SDC_BLMT		BIT(7)		/* TX Big=0 Lil=1 Endian mode */
#define SDMA_SDC_POVR		BIT(8)		/* PCI Override */
#define SDMA_SDC_RIFB		BIT(9)		/* RX Intr on Frame boundaries */
#define SDMA_SDC_RESa		BITS(11,10)
#define SDMA_SDC_BSZ_MASK	BITS(13,12)	/* Maximum Burst Size */
#define SDMA_SDC_BSZ_1x64	(0 << 12)	/* 1 64 bit word */
#define SDMA_SDC_BSZ_2x64	(1 << 12)	/* 2 64 bit words */
#define SDMA_SDC_BSZ_4x64	(2 << 12)	/* 4 64 bit words */
#define SDMA_SDC_BSZ_8x64	(3 << 12)	/* 8 64 bit words */
#define SDMA_SDC_RESb		BITS(31,14)
#define SDMA_SDC_RES (SDMA_SDC_RESa|SDMA_SDC_RESb)
/*
 * SDMA Command Register
 */
#define SDMA_SDCM_RESa		BITS(6,0)
#define SDMA_SDCM_ERD		BIT(7)		/* Enable RX DMA */
#define SDMA_SDCM_RESb		BITS(14,8)
#define SDMA_SDCM_AR		BIT(15)		/* Abort Receive */
#define SDMA_SDCM_STD		BIT(16)		/* Stop TX */
#define SDMA_SDCM_RESc		BITS(22,17)
#define SDMA_SDCM_TXD		BIT(23)		/* TX Demand */
#define SDMA_SDCM_RESd		BITS(30,24)
#define SDMA_SDCM_AT		BIT(31)		/* Abort TX */
#define SDMA_SDCM_RES \
		(SDMA_SDCM_RESa|SDMA_SDCM_RESb|SDMA_SDCM_RESc|SDMA_SDCM_RESd)
/*
 * SDMA Interrupt Cause and Mask Register bits
 */
#define U__(bits,u)             ((bits) << (((u) % 2) * 8))
#define SDMA_INTR_RXBUF(u)      U__(BIT(0),u)   /* SDMA #0 Rx Buffer Return */
#define SDMA_INTR_RXERR(u)      U__(BIT(1),u)   /* SDMA #0 Rx Error */
#define SDMA_INTR_TXBUF(u)      U__(BIT(2),u)   /* SDMA #0 Tx Buffer Return */
#define SDMA_INTR_TXEND(u)      U__(BIT(3),u)   /* SDMA #0 Tx End */
#define SDMA_INTR_RESa		BITS(7,4)
#define SDMA_INTR_RESb		BITS(31,12)
#define SDMA_INTR_RES           (SDMA_INTR_RESa|SDMA_INTR_RESb)
#define SDMA_U_INTR_MASK(u)     U__(BITS(3,0),u)


/*******************************************************************************
 *
 * SDMA descriptor structure and definitions
 */
/*
 * SDMA descriptor structure used for both TX and RX
 * the `sdma_csr' and `sdma_cnt' fields differ for RX and TX
 * `sdma_csr' varies depending on how it is tasked;
 * see "gtmpscreg.h" for defines on SDMA descriptor CSR values
 * for MPSC UART mode.  Note that pointer fields are physical addrs.
 */
typedef struct sdma_desc {
	uint32_t sdma_cnt;		/* size (rx) or shadow (tx) and count */
	uint32_t sdma_csr;		/* command/status */
	uint32_t sdma_next;		/* next descriptor link */
	uint32_t sdma_bufp;		/* buffer pointer */
} sdma_desc_t;

#define SDMA_RX_CNT_BCNT_SHIFT		0		/* byte count */
#define SDMA_RX_CNT_BCNT_MASK		BITS(15,0)	/*  "    "    */
#define SDMA_RX_CNT_BUFSZ_SHIFT		16		/* buffer size */
#define SDMA_RX_CNT_BUFSZNT_SIZE_MASK	BITS(31,19)	/*  "      "   */
#define SDMA_RX_CNT_BUFP_MASK		BITS(31,3)	/* buffer pointer */
#define SDMA_RX_CNT_NEXT_MASK		BITS(31,4)	/* next desc. pointer */

#define SDMA_TX_CNT_SBC_SHIFT		0		/* shadow byte count */
#define SDMA_TX_CNT_SBC_MASK		BITS(15,0)	/*  "      "    "    */
#define SDMA_TX_CNT_BCNT_SHIFT		16		/* byte count */
#define SDMA_TX_CNT_BCNT_MASK		BITS(31,16	/*  "    "    */
#define SDMA_TX_CNT_NEXT_MASK		BITS(31,4)	/* next desc. pointer */


#endif	/* _GTSDMAREG_H */
