/*	$NetBSD: elinkxlreg.h,v 1.15 2008/04/28 20:23:49 martin Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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
 * This file defines the registers specific to the EtherLink XL family
 * of NICs.
 */

#define EEPROM_SOFTINFO3	0x15	/* Software info #3 */
#define EEPROM_SUBVENDOR_ELXL	0x17	/* Subsys vendor id */
#define EEPROM_SUBSYSID		0x18	/* Subsys id */
#define EEPROM_MEDIA		0x19	/* Media options (90xB) */
#define EEPROM_CHECKSUM_ELXL	0x20	/* EEPROM checksum */

#define READ_EEPROM8		0x0200	/* 8 bit EEPROM read command */

/*
 * Flat address space registers (outside the windows)
 */

#define ELINK_TXPKTID		0x18	/* 90xB only */
#define ELINK_TIMER		0x1a
#define ELINK_TXSTATUS		0x1b
#define ELINK_INTSTATUSAUTO	0x1e
#define ELINK_DMACTRL		0x20
#	define ELINK_DMAC_DNCMPLREQ	0x00000002
#	define ELINK_DMAC_DNSTALLED	0x00000004
#	define ELINK_DMAC_UPCOMPLETE	0x00000008
#	define ELINK_DMAC_DNCOMPLETE	0x00000010
#	define ELINK_DMAC_UPRXEAREN	0x00000020
#	define ELINK_DMAC_ARNCNTDN	0x00000040
#	define ELINK_DMAC_DNINPROG	0x00000080
#	define ELINK_DMAC_CNTSPEED	0x00000100
#	define ELINK_DMAC_CNTDNMODE	0x00000200
#	define ELINK_DMAC_ALTSEQDIS	0x00010000
#	define ELINK_DMAC_DEFEATMWI	0x00100000
#	define ELINK_DMAC_DEFEATMRL	0x00200000
#	define ELINK_DMAC_UPOVERDIS	0x00400000
#	define ELINK_DMAC_TARGABORT	0x40000000
#	define ELINK_DMAC_MSTRABORT	0x80000000
#define ELINK_DNLISTPTR		0x24
#define ELINK_DNBURSTTHRESH	0x2a	/* 90xB only */
#define ELINK_DNPRIOTHRESH	0x2c	/* 90xB only */
#define ELINK_DNPOLL		0x2d	/* 90xB only */
#define ELINK_TXFREETHRESH	0x2f	/* 90x only */
#define ELINK_UPPKTSTATUS	0x30
#define ELINK_FREETIMER		0x34
#define ELINK_COUNTDOWN		0x36
#define ELINK_UPLISTPTR		0x38
#define ELINK_UPPRIOTHRESH	0x3c	/* 90xB only */
#define ELINK_UPPOLL		0x3d	/* 90xB only */
#define ELINK_UPBURSTTHRESH	0x3e	/* 90xB only */
#define ELINK_REALTIMECNT	0x40	/* 90xB only */
#define ELINK_DNMAXBURST	0x78	/* 90xB only */
#define ELINK_UPMAXBURST	0x7a	/* 90xB only */

/*
 * This is reset options for the other cards, media options for
 * the 90xB NICs. Reset options are in a separate register for
 * the 90xB.
 */
#define ELINK_W3_MEDIA_OPTIONS	0x08
#	define ELINK_MEDIACAP_100BASET4	0x0001
#	define ELINK_MEDIACAP_100BASETX	0x0002
#	define ELINK_MEDIACAP_100BASEFX	0x0004
#	define ELINK_MEDIACAP_10BASET	0x0008
#	define ELINK_MEDIACAP_10BASE2	0x0010
#	define ELINK_MEDIACAP_10BASE5	0x0020
#	define ELINK_MEDIACAP_MII	0x0040
#	define ELINK_MEDIACAP_10BASEFL	0x0080

/*
 * Reset options for the 90xB
 */
#define ELINK_W2_RESET_OPTIONS	0x0c
#	define ELINK_RESET_OPT_LEDPOLAR	0x0010
#	define ELINK_RESET_OPT_PHYPOWER	0x4000

/*
 * Window 4, offset 8 is defined for MII/PHY access for EtherLink XL
 * cards.
 */
#define ELINK_W4_PHYSMGMT	0x08
#	define ELINK_PHY_CLK	0x0001
#	define ELINK_PHY_DATA	0x0002
#	define ELINK_PHY_DIR	0x0004

/*
 * Counter in window 4 for packets with a bad start-of-stream delimiter/
 */
#define ELINK_W4_BADSSD		0x0c
#define ELINK_W4_UBYTESOK	0x0d

/*
 * Define for extra multicast hash filter bit implemented in the 90xB
 */
#define FIL_MULTIHASH		0x10

/*
 * Defines for the interrupt status register, only for the 90x[B]
 */
#define HOST_ERROR		0x0002
#define LINK_EVENT		0x0100
#define DN_COMPLETE		0x0200
#define UP_COMPLETE		0x0400

#define XL_WATCHED_INTERRUPTS \
    (HOST_ERROR | TX_COMPLETE | UPD_STATS | DN_COMPLETE | UP_COMPLETE)


/*
 * Window 7 registers. These are different for 90x and 90xB than
 * for the EtherLink III / Fast EtherLink cards.
 */

#define ELINK_W7_VLANMASK	0x00	/* 90xB only */
#define ELINK_W7_VLANTYPE	0x04	/* 90xB only */
#define ELINK_W7_TIMER		0x0a	/* 90x only */
#define ELINK_W7_TX_STATUS	0x0b	/* 90x only */
#define ELINK_W7_POWEREVENT	0x0c	/* 90xB only */
#define ELINK_W7_INTSTATUS	0x0e

/*
 * Command definitions.
 */
#define ELINK_UPSTALL		0x3000
#define ELINK_UPUNSTALL		0x3001
#define ELINK_DNSTALL		0x3002
#define ELINK_DNUNSTALL		0x3003
#define ELINK_TXRECLTHRESH	0xc000
#define ELINK_TXSTARTTHRESH	0x9800
#define ELINK_CLEARHASHFILBIT	0xc800
#define ELINK_SETHASHFILBIT	0xcc00

/*
 * The Internal Config register is different on 90xB cards. The
 * different masks / shifts are defined here.
 */

/*
 * Lower 16 bits.
 */
#define CONFIG_TXLARGE		(u_int16_t) 0x4000
#define CONFIG_TXLARGE_SHIFT	14

#define CONFIG_RXLARGE		(u_int16_t) 0x8000
#define CONFIG_RXLARGE_SHIFT	15

/*
 * Upper 16 bits.
 */
#define CONFIG_XCVR_SEL		(u_int16_t) 0x00f0
#define CONFIG_XCVR_SEL_SHIFT	4

#define	ELINKMEDIA_AUTO		8

#define CONFIG_AUTOSEL		(u_int16_t) 0x0100
#define CONFIG_AUTOSEL_SHIFT	8

#define CONFIG_DISABLEROM	(u_int16_t) 0x0200
#define CONFIG_DISABLEROM_SHIFT	9

/*
 * ID of internal PHY.
 */

#define ELINK_INTPHY_ID		24

/*
 * Fragment header as laid out in memory for DMA access.
 */

struct ex_fraghdr {
	volatile u_int32_t fr_addr;	/* phys addr of frag */
	volatile u_int32_t fr_len;	/* length of frag */
};

#define EX_FR_LENMASK	0x00001fff	/* mask for length in fr_len field */
#define EX_FR_LAST	0x80000000	/* indicates last fragment */

#define EX_NDPD		256
#define EX_NUPD		128

/*
 * Note: the number of receive fragments in an UPD is 1, since we're
 * receiving into one contiguous mbuf.
 */
#define EX_NRFRAGS	1		/* # fragments in rcv pkt (< 64) */
#define EX_NTFRAGS	32		/* # fragments in tx pkt (< 64) */

/*
 * Type 0 Download Packet Descriptor (DPD).
 */
struct ex_dpd {
	volatile u_int32_t dpd_nextptr;		/* prt to next fragheader */
	volatile u_int32_t dpd_fsh;		/* frame start header */
	volatile struct ex_fraghdr dpd_frags[EX_NTFRAGS];
};

/*
 * Type 1 DPD, supported by 90xB.
 */
struct ex_dpd1 {
	volatile u_int32_t dpd_nextptr;
	volatile u_int32_t dpd_schedtime;	/* time to download */
	volatile u_int32_t dpd_fsh;
	volatile struct ex_fraghdr dpd_frags[EX_NTFRAGS];
};

struct ex_upd {
	volatile u_int32_t upd_nextptr;
	volatile u_int32_t upd_pktstatus;
	volatile struct ex_fraghdr upd_frags[EX_NRFRAGS];
};

/*
 * Higher level linked list of upload packet descriptors.
 */
struct ex_rxdesc {
	struct ex_rxdesc *rx_next;
	struct mbuf *rx_mbhead;
	bus_dmamap_t rx_dmamap;
	struct ex_upd *rx_upd;
};

/*
 * .. and for download packet descriptors.
 */
struct ex_txdesc {
	struct ex_txdesc *tx_next;
	struct mbuf *tx_mbhead;
	bus_dmamap_t tx_dmamap;
	struct ex_dpd *tx_dpd;
};

/*
 * hardware ip4csum-tx on ex(4) sometimes seems to set wrong IP checksums
 * if the TX IP packet length is 21 or 22 bytes which requires autopadding.
 * To avoid this bug, we have to pad such very short packets manually.
 */
#define EX_IP4CSUMTX_MINLEN	22
#define EX_IP4CSUMTX_PADLEN	(ETHER_HDR_LEN + EX_IP4CSUMTX_MINLEN)

#define DPDMEM_SIZE		(sizeof(struct ex_dpd) * EX_NDPD)
#define DPDMEMPAD_OFF		DPDMEM_SIZE
#define DPDMEMPAD_DMADDR(sc)	((sc)->sc_dpddma + DPDMEMPAD_OFF)

#define DPD_DMADDR(s,t) \
	((s)->sc_dpddma + ((char *)((t)->tx_dpd) - (char *)((s)->sc_dpd)))

/*
 * Frame Start Header bitfields.
 */

#define EX_DPD_DNIND	0x80000000	/* intr on download done */
#define EX_DPD_TXIND	0x00008000	/* intr on tx done */
#define EX_DPD_NOCRC	0x00002000	/* no CRC append */

/*
 * Lower 12 bits are the tx length for the 90x family. The 90xB
 * assumes that the tx length is the sum of all frame lengths,
 * and uses the bits as below. It also defines some more bits in
 * the upper part.
 */
#define EX_DPD_EMPTY	0x20000000	/* no data in this DPD */
#define EX_DPD_UPDEFEAT	0x10000000	/* don't round tx lengths up */
#define EX_DPD_UDPCKSUM	0x08000000	/* do hardware UDP checksum */
#define EX_DPD_TCPCKSUM	0x04000000	/* do hardware TCP checksum */
#define EX_DPD_IPCKSUM	0x02000000	/* do hardware IP checksum */
#define EX_DPD_DNCMPLT	0x01000000	/* packet has been downloaded */
#define EX_DPD_IDMASK	0x000003fc	/* mask for packet id */
#	define EX_DPD_IDSHIFT	2
#define EX_DPD_RNDMASK	0x00000003	/* mask for rounding */
					/* 0 -> dword, 2 -> word, 1,3 -> none */

/*
 * Schedtime bitfields.
 */
#define EX_SCHED_TIMEVALID	0x20000000	/* field contains value */
#define EX_SCHED_LDCOUNT	0x10000000	/* load schedtime onto NIC */
#define EX_SCHED_TIMEMASK	0x00ffffff

/*
 * upd_pktstatus bitfields.
 * The *CKSUMERR fields are only valid if the matching *CHECKED field
 * is set.
 */
#define EX_UPD_PKTLENMASK	0x00001fff	/* 12:0 -> packet length */
#define EX_UPD_ERROR		0x00004000	/* rcv error */
#define EX_UPD_COMPLETE		0x00008000	/* rcv complete */
#define EX_UPD_OVERRUN		0x00010000	/* rcv overrun */
#define EX_UPD_RUNT		0x00020000	/* pkt < 60 bytes */
#define EX_UPD_ALIGNERR		0x00040000	/* alignment error */
#define EX_UPD_CRCERR		0x00080000	/* CRC error */
#define EX_UPD_OVERSIZED	0x00100000	/* oversize frame */
#define EX_UPD_DRIBBLEBITS	0x00800000	/* pkt had dribble bits */
#define EX_UPD_OVERFLOW		0x01000000	/* insufficient space for pkt */
#define EX_UPD_IPCKSUMERR	0x02000000	/* IP cksum error (90xB) */
#define EX_UPD_TCPCKSUMERR	0x04000000	/* TCP cksum error (90xB) */
#define EX_UPD_UDPCKSUMERR	0x08000000	/* UDP cksum error (90xB) */
#define EX_UPD_IPCHECKED	0x20000000	/* IP cksum done */
#define EX_UPD_TCPCHECKED	0x40000000	/* TCP cksum done */
#define EX_UPD_UDPCHECKED	0x80000000	/* UDP cksum done */

#define EX_UPD_ERR		0x001f4000	/* Errors we check for */
#define EX_UPD_ERR_VLAN		0x000f0000	/* same for 802.1q */
