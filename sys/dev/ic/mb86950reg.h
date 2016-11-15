/*	$NetBSD: mb86950reg.h,v 1.3 2005/12/11 12:21:27 christos Exp $	*/

/*
 * Copyright (c) 1995 Mika Kortelainen
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
 *      This product includes software developed by  Mika Kortelainen
 * 4. The name of the author may not be used to endorse or promote products
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
 * Adapted from if_qnreg.h for the amiga port of NetBSD by Dave J. Barnes, 2004.
 */

/*
 * The Fujitsu mb86950, "EtherStar", is the predecessor to the mb8696x
 * NICE supported by the ate driver.  While similar in function and
 * programming to the mb8696x, the register offset differences and
 * quirks make it nearly impossible to have one driver for both the
 * EtherStar and NICE chips.
 *
 *  Definitions from Fujitsu documentation.
 */

#define ESTAR_DLCR0		0 /* Transmit status */
#define DLCR_TX_STAT	ESTAR_DLCR0

#define ESTAR_DLCR1		1 /* Transmit masks  */
#define DLCR_TX_INT_EN	ESTAR_DLCR1

/* DLCR0/1 - Transmit Status & Masks */
#define TX_DONE			0x80 /* Transmit okay         */
/* bit 6 - Net Busy, carrier sense ? */
/* bit 5 - Transmit packet received ? */
#define TX_CR_LOST		0x10 /* Carrier lost while attempting to transmit */
#define TX_UNDERFLO		0x08 /* fifo underflow */
#define TX_COL			0x04 /* Collision             */
#define TX_16COL		0x02 /* 16 collision          */
#define TX_BUS_WR_ERR	0x01 /* Bus write error, fifo overflo */
#define CLEAR_TX_ERR	(TX_UNDERFLO | TX_COL | TX_16COL | TX_BUS_WR_ERR) /* Clear transmit errors */
#define TX_MASK         (TX_DONE | TX_16COL)

#define ESTAR_DLCR2		2 /* Receive status  */
#define DLCR_RX_STAT	ESTAR_DLCR2

#define ESTAR_DLCR3		3 /* Receive masks   */
#define DLCR_RX_INT_EN	ESTAR_DLCR3

/* DLCR2/3 - Receive Status & Masks */
#define RX_PKT			0x80 /* Packet ready          */
#define RX_BUS_RD_ERR   0x40 /* fifo underflow, harmless, normally masked off */
/* bit 5 - DMA end of process ? */
/* bit 4 - remote control packet rx ? */
#define RX_SHORT_ERR	0x08 /* Short packet          */
#define RX_ALIGN_ERR	0x04 /* Alignment error       */
#define RX_CRC_ERR		0x02 /* CRC error             */
#define RX_OVERFLO		0x01 /* Receive buf overflow  */
#define CLEAR_RX_ERR	RX_MASK /* Clear receive and errors  */
#define	RX_MASK			(RX_PKT | RX_SHORT_ERR | RX_ALIGN_ERR | RX_CRC_ERR | RX_OVERFLO | RX_BUS_RD_ERR)
#define RX_ERR_MASK  	(RX_SHORT_ERR | RX_ALIGN_ERR | RX_CRC_ERR | RX_OVERFLO | RX_BUS_RD_ERR)

#define ESTAR_DLCR4		4 /* Transmit mode   */
#define DLCR_TX_MODE	ESTAR_DLCR4

/* DLCR4 - Transmit Mode */
/* bits 7, 6, 5, 4 - collision count ? */
#define COL_MASK		0xf0
/* bit 3 - nc */
/* bit 2 - gen output ? */
#define LBC				0x02 /* Loopback control      */
/* bit 0 - defer ?, normally 0 */

#define ESTAR_DLCR5		5 /* Receive mode    */
#define DLCR_RX_MODE	ESTAR_DLCR5

/* DLCR5 - Receive Mode */
/* Normal mode: accept physical address, broadcast address.
 */
/* bit 7 - Disable CRC test mode */
#define RX_BUF_EMTY		0x40 /* Buffer empty          */
/* bit 5 - accept packet with errors or nc ?, normally set to 0 */
/* bit 4 - 40 bit address ?, normally set to 0 */
/* bit 3 - accept runts ?, normally set to 0 */
/* bit 2 - remote reset ? normally set to 0 */
/* bit 1 & 0 - address filter mode */

/*  00 = reject */
/*  01 = normal mode */
#define NORMAL_MODE		0x01
/*  10 = ? */
/*  11 = promiscuous mode */
#define PROMISCUOUS_MODE	0x03 /* Accept all packets    */

#define ESTAR_DLCR6		6 /* Software reset  */
#define DLCR_CONFIG		ESTAR_DLCR6

/* DLCR6 - Enable Data Link Controller */
#define DISABLE_DLC		0x80 /* Disable data link controller */
#define ENABLE_DLC		0x00 /* Enable data link controller  */

#define ESTAR_DLCR7		7 /* TDR (LSB)       */

#define ESTAR_DLCR8		8 /* Node ID0        */
#define DLCR_NODE_ID	ESTAR_DLCR8

#define ESTAR_DLCR9		9 /* Node ID1        */
#define ESTAR_DLCR10	10 /* Node ID2        */
#define ESTAR_DLCR11	11 /* Node ID3        */
#define ESTAR_DLCR12	12 /* Node ID4        */
#define ESTAR_DLCR13	13 /* Node ID5        */

#define ESTAR_DLCR15	15 /* TDR (MSB)       */

/* The next three are usually accessed as words */
#define ESTAR_BMPR0		16 /* Buffer memory port (FIFO) */
#define BMPR_FIFO		ESTAR_BMPR0

#define ESTAR_BMPR2		18 /* Packet length   */
#define BMPR_TX_LENGTH	ESTAR_BMPR2
/* BMPR2:BMPR3 - Packet Length Registers (Write-only) */
#define TRANSMIT_START	0x8000

#define ESTAR_BMPR4		20 /* DMA enable      */
#define BMPR_DMA		ESTAR_BMPR4
