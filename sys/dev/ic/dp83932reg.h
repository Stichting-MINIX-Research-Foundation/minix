/*	$NetBSD: dp83932reg.h,v 1.7 2008/04/28 20:23:49 martin Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

#ifndef _DEV_IC_DP83932REG_H_
#define	_DEV_IC_DP83932REG_H_

/*
 * Register description for the National Semiconductor DP83932
 * Systems-Oriented Network Interface Controller (SONIC).
 */

/*
 * SONIC Receive Descriptor Area.
 */
struct sonic_rda16 {
	uint16_t	rda_status;
	uint16_t	rda_bytecount;
	uint16_t	rda_pkt_ptr0;
	uint16_t	rda_pkt_ptr1;
	uint16_t	rda_seqno;
	uint16_t	rda_link;
	uint16_t	rda_inuse;
} __packed;

struct sonic_rda32 {
	uint32_t	rda_status;
	uint32_t	rda_bytecount;
	uint32_t	rda_pkt_ptr0;
	uint32_t	rda_pkt_ptr1;
	uint32_t	rda_seqno;
	uint32_t	rda_link;
	uint32_t	rda_inuse;
} __packed;

#define	RDA_SEQNO_RBA(x)	(((x) >> 8) & 0xff)
#define	RDA_SEQNO_RSN(x)	((x) & 0xff)

#define	RDA_LINK_EOL	0x01		/* end-of-list */

/*
 * SONIC Receive Resource Area.
 *
 * Note, in 32-bit mode, Rx buffers must be aligned to 32-bit
 * boundaries, and in 16-bit mode, to 16-bit boundaries.
 *
 * Also note the `word count' is always in units of 16-bit words.
 */
struct sonic_rra16 {
	uint16_t	rra_ptr0;
	uint16_t	rra_ptr1;
	uint16_t	rra_wc0;
	uint16_t	rra_wc1;
} __packed;

struct sonic_rra32 {
	uint32_t	rra_ptr0;
	uint32_t	rra_ptr1;
	uint32_t	rra_wc0;
	uint32_t	rra_wc1;
} __packed;

/*
 * SONIC Transmit Descriptor Area
 *
 * Note the number of fragments defined here is arbitrary.
 */
#define	SONIC_NTXFRAGS	16

struct sonic_frag16 {
	uint16_t	frag_ptr0;
	uint16_t	frag_ptr1;
	uint16_t	frag_size;
} __packed;

struct sonic_frag32 {
	uint32_t	frag_ptr0;
	uint32_t	frag_ptr1;
	uint32_t	frag_size;
} __packed;

/*
 * Note the frag after the last frag is used to link up to the
 * next descriptor.
 */

struct sonic_tda16 {
	uint16_t	tda_status;
	uint16_t	tda_pktconfig;
	uint16_t	tda_pktsize;
	uint16_t	tda_fragcnt;
	struct sonic_frag16 tda_frags[SONIC_NTXFRAGS + 1];
#if 0
	uint16_t	tda_link;
#endif
} __packed;

struct sonic_tda32 {
	uint32_t	tda_status;
	uint32_t	tda_pktconfig;
	uint32_t	tda_pktsize;
	uint32_t	tda_fragcnt;
	struct sonic_frag32 tda_frags[SONIC_NTXFRAGS + 1];
#if 0
	uint32_t	tda_link;
#endif
} __packed;

#define	TDA_STATUS_NCOL(x)	(((x) >> 11) & 0x1f)

#define	TDA_LINK_EOL		0x01	/* end-of-list */

/*
 * SONIC CAM Descriptor Area.
 */
struct sonic_cda16 {
	uint16_t	cda_entry;
	uint16_t	cda_addr0;
	uint16_t	cda_addr1;
	uint16_t	cda_addr2;
} __packed;

struct sonic_cda32 {
	uint32_t	cda_entry;
	uint32_t	cda_addr0;
	uint32_t	cda_addr1;
	uint32_t	cda_addr2;
} __packed;

/*
 * SONIC register file.
 *
 * NOTE: We define these as indices, and use a register map to deal
 * with different address strides.
 */

#define	SONIC_CR	0x00	/* Command Register */
#define	CR_HTX		(1U << 0)	/* Halt Transmission */
#define	CR_TXP		(1U << 1)	/* Transmit Packets */
#define	CR_RXDIS	(1U << 2)	/* Receiver Disable */
#define	CR_RXEN		(1U << 3)	/* Receiver Enable */
#define	CR_STP		(1U << 4)	/* Stop Timer */
#define	CR_ST		(1U << 5)	/* Start Timer */
#define	CR_RST		(1U << 7)	/* Software Reset */
#define	CR_RRRA		(1U << 8)	/* Read RRA */
#define	CR_LCAM		(1U << 9)	/* Load CAM */

#define	SONIC_DCR	0x01	/* Data Configuration Register */
#define	DCR_TFT0	(1U << 0)	/* Transmit FIFO Threshold (lo) */
#define	DCR_TFT1	(1U << 1)	/* Transmit FIFO Threshold (hi) */
#define	DCR_RFT0	(1U << 2)	/* Receive FIFO Threshold (lo) */
#define	DCR_RFT1	(1U << 3)	/* Receive FIFO Threshold (hi) */
#define	DCR_BMS		(1U << 4)	/* Block Mode Select for DMA */
#define	DCR_DW		(1U << 5)	/* Data Width Select */
#define	DCR_WC0		(1U << 6)	/* Wait State Control (lo) */
#define	DCR_WC1		(1U << 7)	/* Wait State Control (hi) */
#define	DCR_USR0	(1U << 8)	/* User Definable Pin 0 */
#define	DCR_USR1	(1U << 9)	/* User Definable Pin 1 */
#define	DCR_SBUS	(1U << 10)	/* Synchronous Bus Mode */
#define	DCR_PO0		(1U << 11)	/* Programmable Output 0 */
#define	DCR_PO1		(1U << 12)	/* Programmable Output 1 */
#define	DCR_LBR		(1U << 13)	/* Latched Bus Retry */
#define	DCR_EXBUS	(1U << 15)	/* Extended Bus Mode */

#define	SONIC_RCR	0x02	/* Receive Control Register */
#define	RCR_PRX		(1U << 0)	/* Packet Received OK */
#define	RCR_LBK		(1U << 1)	/* Loopback Packet Received */
#define	RCR_FAER	(1U << 2)	/* Frame Alignment Error */
#define	RCR_CRCR	(1U << 3)	/* CRC Error */
#define	RCR_COL		(1U << 4)	/* Collision Activity */
#define	RCR_CRS		(1U << 5)	/* Carrier Sense Activity */
#define	RCR_LPKT	(1U << 6)	/* Last Packet in RBA */
#define	RCR_BC		(1U << 7)	/* Broadcast Packet Received */
#define	RCR_MC		(1U << 8)	/* Multicast Packet Received */
#define	RCR_LB0		(1U << 9)	/* Loopback Control 0 */
#define	RCR_LB1		(1U << 10)	/* Loopback Control 1 */
#define	RCR_AMC		(1U << 11)	/* Accept All Multicast Packets */
#define	RCR_PRO		(1U << 12)	/* Physical Promiscuous Packets */
#define	RCR_BRD		(1U << 13)	/* Accept Broadcast Packets */
#define	RCR_RNT		(1U << 14)	/* Accept Runt Packets */
#define	RCR_ERR		(1U << 15)	/* Accept Packets with Errors */

#define	SONIC_TCR	0x03	/* Transmit Control Register */
#define	TCR_PTX		(1U << 0)	/* Packet Transmitted OK */
#define	TCR_BCM		(1U << 1)	/* Byte Count Mismatch */
#define	TCR_FU		(1U << 2)	/* FIFO Underrun */
#define	TCR_PMB		(1U << 3)	/* Packet Monitored Bad */
#define	TCR_OWC		(1U << 5)	/* Out of Window Collision */
#define	TCR_EXC		(1U << 6)	/* Excessive Collisions */
#define	TCR_CRSL	(1U << 7)	/* Carrier Sense Lost */
#define	TCR_NCRS	(1U << 8)	/* No Carrier Sense */
#define	TCR_DEF		(1U << 9)	/* Deferred Transmission */
#define	TCR_EXD		(1U << 10)	/* Excessive Deferral */
#define	TCR_EXDIS	(1U << 12)	/* Disable Excessive Deferral Timer */
#define	TCR_CRCI	(1U << 13)	/* CRC Inhibit */
#define	TCR_POWC	(1U << 14)	/* Programmed Out of Window Col. Tmr */
#define	TCR_PINT	(1U << 15)	/* Programmable Interrupt */

#define	SONIC_IMR	0x04	/* Interrupt Mask Register */
#define	IMR_RFO		(1U << 0)	/* Rx FIFO Overrun */
#define	IMR_MP		(1U << 1)	/* Missed Packet Tally */
#define	IMR_FAE		(1U << 2)	/* Frame Alignment Error Tally */
#define	IMR_CRC		(1U << 3)	/* CRC Tally */
#define	IMR_RBA		(1U << 4)	/* RBA Exceeded */
#define	IMR_RBE		(1U << 5)	/* Rx Buffers Exhausted */
#define	IMR_RDE		(1U << 6)	/* Rx Descriptors Exhausted */
#define	IMR_TC		(1U << 7)	/* Timer Complete */
#define	IMR_TXER	(1U << 8)	/* Transmit Error */
#define	IMR_PTX		(1U << 9)	/* Transmit OK */
#define	IMR_PRX		(1U << 10)	/* Packet Received */
#define	IMR_PINT	(1U << 11)	/* Programmable Interrupt */
#define	IMR_LCD		(1U << 12)	/* Load CAM Done */
#define	IMR_HBL		(1U << 13)	/* Heartbeat Lost */
#define	IMR_BR		(1U << 14)	/* Bus Retry Occurred */

#define	IMR_ALL		0x7fff

#define	SONIC_ISR	0x05	/* Interrupt Status Register */
	/* See IMR bits. */

#define	SONIC_UTDAR	0x06	/* Upper Tx Descriptor Address Register */

#define	SONIC_CTDAR	0x07	/* Current Tx Descriptor Address Register */

#define	SONIC_TPS	0x08	/* Transmit Packet Size */

#define	SONIC_TFC	0x09	/* Transmit Fragment Count */

#define	SONIC_TSA0	0x0a	/* Transmit Start Address (lo) */

#define	SONIC_TSA1	0x0b	/* Transmit Start Address (hi) */

#define	SONIC_TFS	0x0c	/* Transmit Fragment Size */

#define	SONIC_URDAR	0x0d	/* Upper Rx Descriptor Address Register */

#define	SONIC_CRDAR	0x0e	/* Current Rx Descriptor Address Register */

#define	SONIC_CRBA0	0x0f	/* Current Receive Buffer Address (lo) */

#define	SONIC_CRBA1	0x10	/* Current Receive Buffer Address (hi) */

#define	SONIC_RBWC0	0x11	/* Remaining Buffer Word Count 0 */

#define	SONIC_RBWC1	0x12	/* Remaining Buffer Word Count 1 */

#define	SONIC_EOBC	0x13	/* End Of Buffer Word Count */

#define	SONIC_URRAR	0x14	/* Upper Rx Resource Address Register */

#define	SONIC_RSAR	0x15	/* Resource Start Address Register */

#define	SONIC_REAR	0x16	/* Resource End Address Register */

#define	SONIC_RRR	0x17	/* Resource Read Register */

#define	SONIC_RWR	0x18	/* Resource Write Register */

#define	SONIC_TRBA0	0x19	/* Temporary Receive Buffer Address (lo) */

#define	SONIC_TRBA1	0x1a	/* Temporary Receive Buffer Address (hi) */

#define	SONIC_TBWC0	0x1b	/* Temporary Buffer Word Count 0 */

#define	SONIC_TBWC1	0x1c	/* Temporary Buffer Word Count 1 */

#define	SONIC_ADDR0	0x1d	/* Address Generator 0 */

#define	SONIC_ADDR1	0x1e	/* Address Generator 1 */

#define	SONIC_LLFA	0x1f	/* Last Link Field Address */

#define	SONIC_TTDA	0x20	/* Temporary Tx Descriptor Address */

#define	SONIC_CEP	0x21	/* CAM Entry Pointer */

#define	SONIC_CAP2	0x22	/* CAM Address Port 2 */

#define	SONIC_CAP1	0x23	/* CAM Address Port 1 */

#define	SONIC_CAP0	0x24	/* CAM Address Port 0 */

#define	SONIC_CER	0x25	/* CAM Enable Register */

#define	SONIC_CDP	0x26	/* CAM Descriptor Pointer */

#define	SONIC_CDC	0x27	/* CAM Descriptor Count */

#define	SONIC_SRR	0x28	/* Silicon Revision Register */

#define	SONIC_WT0	0x29	/* Watchdog Timer 0 */

#define	SONIC_WT1	0x2a	/* Watchdog Timer 1 */

#define	SONIC_RSC	0x2b	/* Receive Sequence Counter */

#define	SONIC_CRCETC	0x2c	/* CRC Error Tally Count */

#define	SONIC_FAET	0x2d	/* Frame Alignment Error Tally */

#define	SONIC_MPT	0x2e	/* Missed Packet Tally */

#define	SONIC_DCR2	0x3f	/* Data Configuration Register 2 */
#define	DCR2_RJCM	(1U << 0)	/* Reject on CAM Match */
#define	DCR2_PCNM	(1U << 1)	/* Packet Compress When not Matched */
#define	DCR2_PCM	(1U << 2)	/* Packet Compress When Matched */
#define	DCR2_PH		(1U << 4)	/* Program Hold */
#define	DCR2_EXPO0	(1U << 12)	/* Extended Programmable Output 0 */
#define	DCR2_EXPO1	(1U << 13)	/* Extended Programmable Output 1 */
#define	DCR2_EXPO2	(1U << 14)	/* Extended Programmable Output 2 */
#define	DCR2_EXPO3	(1U << 15)	/* Extended Programmable Output 3 */

#define	SONIC_NREGS	0x40

#endif /* _DEV_IC_DP83932REG_H_ */
