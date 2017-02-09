/*	$NetBSD: gtintrreg.h,v 1.5 2010/04/28 13:51:56 kiyohara Exp $	*/

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
 * gt64260intr.h: defines for GT-64260 system controller interrupts
 *
 * creation	Sun Jan  7 18:05:59 PST 2001	cliff
 *
 * NOTE:
 *	Galileo GT-64260 manual bit defines assume Little Endian
 *	ordering of bits within bytes, i.e.
 *		bit #0 --> 0x01
 *	vs. Motorola Big Endian bit numbering where
 *		bit #0 --> 0x80
 *	Consequently we define bits in Little Endian format and plan
 *	to swizzle bytes during programmed I/O by using lwbrx/swbrx
 *	to load/store GT-64260 registers.
 */


#ifndef _DISCOVERY_GT64260INTR_H
#define _DISCOVERY_GT64260INTR_H


/*
 * GT-64260 Interrupt Controller Register Map
 */
#define ICR_MIC_LO	0xc18	/* main interrupt cause low */
#define ICR_MIC_HI	0xc68	/* main interrupt cause high */
#define ICR_CIM_LO	0xc1c	/* CPU interrupt mask low */
#define ICR_CIM_HI	0xc6c	/* CPU interrupt mask high */
#define ICR_CSC		0xc70	/* CPU select cause */
#define ICR_P0IM_LO	0xc24	/* PCI_0 interrupt mask low */
#define ICR_P0IM_HI	0xc64	/* PCI_0 interrupt mask high */
#define ICR_P0SC	0xc74	/* PCI_0 select cause */
#define ICR_P1IM_LO	0xca4	/* PCI_1 interrupt mask low */
#define ICR_P1IM_HI	0xce4	/* PCI_1 interrupt mask high */
#define ICR_P1SC	0xcf4	/* PCI_1 select cause */
#define ICR_CI0M	0xe60	/* CPU int[0] mask */
#define ICR_CI1M	0xe64	/* CPU int[1] mask */
#define ICR_CI2M	0xe68	/* CPU int[2] mask */
#define ICR_CI3M	0xe6c	/* CPU int[3] mask */

#define IRQ_DEV		1	/* device interface interrupt */
#define IRQ_DMA		2	/* DMA addres error interrupt */
#define IRQ_CPU		3	/* CPU interface interrupt */
#define IRQ_IDMA0_1	4	/* IDMA ch. 0..1 complete interrupt */
#define IRQ_IDMA2_3	5	/* IDMA ch. 2..3 complete interrupt */
#define IRQ_IDMA4_5	6	/* IDMA ch. 4..5 complete interrupt */
#define IRQ_IDMA6_7	7	/* IDMA ch. 6..7 complete interrupt */
#define IRQ_TIME0_1	8	/* Timer 0..1 interrupt */
#define IRQ_TIME2_3	9	/* Timer 2..3 interrupt */
#define IRQ_TIME4_5	10	/* Timer 4..5 interrupt */
#define IRQ_TIME6_7	11	/* Timer 6..7 interrupt */
#define IRQ_PCI0_0	12	/* PCI 0 interrupt 0 summary */
#define IRQ_PCI0_1	13	/* PCI 0 interrupt 1 summary */
#define IRQ_PCI0_2	14	/* PCI 0 interrupt 2 summary */
#define IRQ_PCI0_3	15	/* PCI 0 interrupt 3 summary */
#define IRQ_PCI1_0	16	/* PCI 1 interrupt 0 summary */
#define IRQ_ECC		17	/* ECC error interrupt */
#define IRQ_PCI1_1	18	/* PCI 1 interrupt 1 summary */
#define IRQ_PCI1_2	19	/* PCI 1 interrupt 2 summary */
#define IRQ_PCI1_3	20	/* PCI 1 interrupt 3 summary */
#define IRQ_PCI0OUT_LO	21	/* PCI 0 outbound interrupt summary */
#define IRQ_PCI0OUT_HI	22	/* PCI 0 outbound interrupt summary */
#define IRQ_PCI1OUT_LO	23	/* PCI 1 outbound interrupt summary */
#define IRQ_PCI1OUT_HI	24	/* PCI 1 outbound interrupt summary */
#define IRQ_PCI0IN_LO	26	/* PCI 0 inbound interrupt summary */
#define IRQ_PCI0IN_HI	27	/* PCI 0 inbound interrupt summary */
#define IRQ_PCI1IN_LO	28	/* PCI 1 inbound interrupt summary */
#define IRQ_PCI1IN_HI	29	/* PCI 1 inbound interrupt summary */
#define IRQ_ETH0	32	/* Ethernet controller 0 interrupt */
#define IRQ_ETH1	33	/* Ethernet controller 1 interrupt */
#define IRQ_ETH2	34	/* Ethernet controller 2 interrupt */
#define IRQ_SDMA	36	/* SDMA interrupt */
#define IRQ_I2C		37	/* I2C interrupt */
#define IRQ_BRG		39	/* Baud Rate Generator interrupt */
#define IRQ_MPSC0	40	/* MPSC 0 interrupt */
#define IRQ_MPSC1	42	/* MPSC 1 interrupt */
#define IRQ_COMM	43	/* Comm unit interrupt */
#define IRQ_GPP7_0	56	/* GPP[7..0] interrupt */
#define IRQ_GPP15_8	57	/* GPP[15..8] interrupt */
#define IRQ_GPP23_16	58	/* GPP[23..16] interrupt */
#define IRQ_GPP31_24	59	/* GPP[31..24] interrupt */

#endif	/*  _DISCOVERY_GT64260INTR_H */
