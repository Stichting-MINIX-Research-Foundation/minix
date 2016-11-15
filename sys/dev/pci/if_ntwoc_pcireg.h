/*	$NetBSD: if_ntwoc_pcireg.h,v 1.5 2005/12/11 12:22:49 christos Exp $	*/

/*
 * Copyright (c) 1998 Vixie Enterprises
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Vixie Enterprises nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY VIXIE ENTERPRISES AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL VIXIE ENTERPRISES OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for Vixie Enterprises by Michael Graff
 * <explorer@flame.org>.  To learn more about Vixie Enterprises, see
 * ``http://www.vix.com''.
 */

#ifndef _IF_NTWOC_PCIREG_H_
#define _IF_NTWOC_PCIREG_H_

/* config flags are as follows */
/*
 * 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * +-------------+ +-----+ +-----+ + +---+ +-+     + +---+ +-+   +
 *       tmc         tdiv    rdiv  e1 rxs1 ts1    e0 rxs0  txs0  nports - 1
 */
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

/*
 * ASIC register offsets
 */

/*
 * This register is in the SCA namespace, but is NOT really an SCA register.
 * It contains information about the daughter cards, and provides a method
 * to configure them.
 */
#define NTWOC_FECR	0x200

/*
 * definition of the NTWO_FECR register
 */
#define NTWOC_FECR_ID0		0x0e00	/* mask of daughter card on port 0 */
#define NTWOC_FECR_ID0_SHIFT	     9
#define NTWOC_FECR_ID1		0xe000  /* mask of daughter card on port 1 */
#define NTWOC_FECR_ID1_SHIFT	    13

#define NTWOC_FECR_DTR1		0x0080	/* DTR output for port 1 */
#define NTWOC_FECR_DTR0		0x0040	/* DTR output for port 0 */
#define NTWOC_FECR_DSR1		0x1000	/* DSR input for port 1 */
#define NTWOC_FECR_DSR0		0x0100	/* DSR input for port 0 */
#define NTWOC_FECR_TE1		0x0008	/* tristate enable port 1 */
#define NTWOC_FECR_TE0		0x0004	/* tristate enable port 0 */
#define NTWOC_FECR_ETC1		0x0002	/* output clock port 1 */
#define NTWOC_FECR_ETC0		0x0001	/* output clock port 0 */

/*
 * Daughter card for port.
 */
#define NTWOC_FE_ID_V35		0x00
#define NTWOC_FE_ID_X01		0x01	/* unused? */
#define NTWOC_FE_ID_TEST	0x02
#define NTWOC_FE_ID_X03		0x03	/* unused? */
#define NTWOC_FE_ID_RS232	0x04
#define NTWOC_FE_ID_X05		0x05	/* was hssi, now unused? */
#define NTWOC_FE_ID_RS422	0x06
#define NTWOC_FE_ID_NONE	0x07	/* empty, no card present */

/*
 * ASIC Control defininitions
 */

/* Front End (Modem,etc) Control Register */

#define  ASIC_MODEM   0x200   /* ASIC modem control register Offset */

/* ASIC front end control register bits */
#define  ASIC_DSR1       0x1000    /* DSR signal input port 1 */
#define  ASIC_DSR0        0x100    /* DSR signal input port 0 */
#define  ASIC_DTR1         0x80    /* DTR signal output port 1 */
#define  ASIC_DTR0         0x40    /* DTR signal output port 0 */
#define  ASIC_TE1           0x8    /* RS422 TX,enable port 1 */
#define  ASIC_TE0           0x4    /* RS422 TX,enable port 0 */
#define  ASIC_ETC1          0x2    /* ETC Clock out port 1 */
#define  ASIC_ETC0          0x1    /* ETC Clock out port 0 */

#endif /* _IF_NTWOC_PCIREG_H_ */
