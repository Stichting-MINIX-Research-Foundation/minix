/*	$NetBSD: if_ntwoc_isareg.h,v 1.1 2000/01/04 06:29:21 chopps Exp $ */

/*
 * Copyright (c) 1995 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_ntwoc_isareg.h,v 1.1 2000/01/04 06:29:21 chopps Exp $
 */
#ifndef _IF_NTWOC_ISAREG_H_
#define _IF_NTWOC_ISAREG_H_

#define NTWOC_ISA_NCHAN		2    /* A HD64570 chip have 2 channels */

#define NTWOC_BUF_SIZ		512
#define NTWOC_TX_BLOCKS		2    /* Sepperate sets of tx buffers */

#define NTWOC_CRD_N2		1

/*
 * RISCom/N2 ISA card.
 */
#define NTWOC_SRC_IOPORT_SIZE	0x08	/* also uses 0x8400 -> 0xAxxx */

/* config flags are as follows */
/*
 * 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-------------+ +-----+ +-----+ + +---+ +-+     + +---+ +-+   +
 *       tmc         tdiv    rdiv  e1 rxs1 ts1    e0 rxs0  txs0  nports - 1
 */
#define	NTWOC_FLAGS_NPORT_MASK	0x00000001	/* nports - 1 */
#define	NTWOC_FLAGS_CLK0_MASK	0x000000fc	/* port 0 clock info mask */
#define	NTWOC_FLAGS_CLK1_MASK	0x0000fc00	/* port 1 clock info mask */
#define	NTWOC_FLAGS_RXDIV_MASK	0x000F0000	/* rx div mask */
#define	NTWOC_FLAGS_TXDIV_MASK	0x00F00000	/* tx div mask */
#define	NTWOC_FLAGS_TMC_MASK	0xFF000000	/* tmc port 0 mask */

#define	NTWOC_FLAGS_CLK1_SHIFT	8

/* these are used after you shift down to the clock byte for the resp. port */
#define	NTWOC_FLAGS_TXS_SHIFT		2
#define	NTWOC_FLAGS_TXS_MASK	0x0000000c	/* port 0 tx clk source mask */
#define	NTWOC_FLAGS_TXS_LINE		0	/* use the line clock */
#define	NTWOC_FLAGS_TXS_INTERNAL	1	/* use the internal clock */
#define	NTWOC_FLAGS_TXS_RXCLOCK		2	/* use the receive clock */

#define	NTWOC_FLAGS_RXS_SHIFT		4
#define	NTWOC_FLAGS_RXS_MASK	0x00000070	/* port 0 rx clk source mask */
#define	NTWOC_FLAGS_RXS_LINE		0	/* use the line clock */
#define NTWOC_FLAGS_RXS_LINE_SN		1	/* use line with noise supp. */
#define NTWOC_FLAGS_RXS_INTERNAL	2	/* use internal clock */
#define NTWOC_FLAGS_RXS_ADPLL_OUT	3	/* use brg out for adpll clk */
#define NTWOC_FLAGS_RXS_ADPLL_IN	4	/* use line in for adpll clk */

#define	NTWOC_FLAGS_ECLOCK_SHIFT	7	/* generate external clock */
#define	NTWOC_FLAGS_ECLOCK_MASK	0x00000080	/* port 0 ext clk gen mask */

/* these are used on the flags directly */
#define	NTWOC_FLAGS_RXDIV_SHIFT	16
#define	NTWOC_FLAGS_TXDIV_SHIFT	20
#define	NTWOC_FLAGS_TMC_SHIFT	24


#define NTWOC_PCR		0x00 /* RW, PC Control Register */
#define NTWOC_BAR		0x02 /* RW, Base Address Register */
#define NTWOC_PSR		0x04 /* RW, Page Scan Register */
#define NTWOC_MCR		0x06 /* RW, Modem Control Register */

#define NTWOC_PCR_SCARUN	0x01 /* !Reset */
#define NTWOC_PCR_EN_VPM	0x02 /* Running above 1M */
#define NTWOC_PCR_MEM_WIN	0x04 /* Open memory window */
#define NTWOC_PCR_ISA16		0x08 /* 16 bit ISA mode */
#define NTWOC_PCR_16M_SEL	0xF0 /* A20-A23 Addresses */

#define NTWOC_PSR_PG_SEL	0x1F /* Page 0 - 31 select */
#define NTWOC_PG_MSK		0x1F
#define NTWOC_PSR_WIN_SIZ	0x60 /* Window size select */
#define NTWOC_PSR_WIN_16K	0x00
#define NTWOC_PSR_WIN_32K	0x20
#define NTWOC_PSR_WIN_64K	0x40
#define NTWOC_PSR_WIN_128K	0x60
#define NTWOC_PSR_EN_SCA_DMA	0x80 /* Enable the SCA DMA */

#define NTWOC_MCR_DTR0		0x01 /* Deactivate DTR0 */
#define NTWOC_MCR_DTR1		0x02 /* Deactivate DTR1 */
#define NTWOC_MCR_DSR0		0x04 /* DSR0 Status */
#define NTWOC_MCR_DSR1		0x08 /* DSR1 Status */
#define NTWOC_MCR_TE0		0x10 /* Enable RS422 TXD */
#define NTWOC_MCR_TE1		0x20 /* Enable RS422 TXD */
#define NTWOC_MCR_ETC0		0x40 /* Enable Ext Clock out */
#define NTWOC_MCR_ETC1		0x80 /* Enable Ext Clock out */

#endif /* _IF_NTWOC_ISAREG_H_ */
