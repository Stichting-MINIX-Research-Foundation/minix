/*	$NetBSD: if_mvxpereg.h,v 1.2 2015/06/03 03:55:47 hsuenaga Exp $	*/
/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _IF_MVXPEREG_H_
#define _IF_MVXPEREG_H_

#if BYTE_ORDER == BIG_ENDIAN
#error "BIG ENDIAN not supported"
#endif

#define MVXPE_SIZE		0x4000

#define MVXPE_NWINDOW		6
#define MVXPE_NREMAP		4

#define MVXPE_QUEUE_SIZE	8
#define MVXPE_QUEUE(n)		(1 << (n))
#define MVXPE_QUEUE_ALL		0xff

/*
 * Ethernet Unit Registers
 *  GbE0 BASE 0x00007.0000 SIZE 0x4000
 *  GbE1 BASE 0x00007.4000 SIZE 0x4000
 *
 * TBD: reasonable bus space submapping....
 */
/* Address Decoder Registers */
#define MVXPE_BASEADDR(n)	(0x2200 + ((n) << 3))	/* Base Address */
#define MVXPE_S(n)		(0x2204 + ((n) << 3))	/* Size */
#define MVXPE_HA(n)		(0x2280 + ((n) << 2))	/* High Address Remap */
#define MVXPE_BARE 		0x2290	/* Base Address Enable */
#define MVXPE_EPAP 		0x2294	/* Ethernet Port Access Protect */

/* Global Miscellaneous Registers */
#define MVXPE_PHYADDR		0x2000
#define MVXPE_SMI		0x2004
#define MVXPE_EUDA		0x2008	/* Ethernet Unit Default Address */
#define MVXPE_EUDID		0x200c	/* Ethernet Unit Default ID */
#define MVXPE_EUIC 		0x2080	/* Ethernet Unit Interrupt Cause */
#define MVXPE_EUIM 		0x2084	/* Ethernet Unit Interrupt Mask */
#define MVXPE_EUEA 		0x2094	/* Ethernet Unit Error Address */
#define MVXPE_EUIAE 		0x2098	/* Ethernet Unit Internal Addr Error */
#define MVXPE_EUC 		0x20b0	/* Ethernet Unit Control */

/* Miscellaneous Registers */
#define MVXPE_SDC		0x241c	/* SDMA Configuration */

/* Networking Controller Miscellaneous Registers */
#define MVXPE_PACC		0x2500	/* Port Acceleration Mode */
#define MVXPE_PV		0x25bc	/* Port Version */

/* Rx DMA Hardware Parser Registers */
#define MVXPE_EVLANE		0x2410	/* VLAN EtherType */
#define MVXPE_MACAL		0x2414	/* MAC Address Low */
#define MVXPE_MACAH		0x2418	/* MAC Address High */
#define MVXPE_NDSCP		7
#define MVXPE_DSCP(n)		(0x2420 + ((n) << 2))
#define MVXPE_VPT2P		0x2440	/* VLAN Priority Tag to Priority */
#define MVXPE_ETP		0x24bc	/* Ethernet Type Priority */
#define MVXPE_NDFSMT		64
#define MVXPE_DFSMT(n)		(0x3400 + ((n) << 2))
			/* Destination Address Filter Special Multicast Table */
#define MVXPE_NDFOMT		64
#define MVXPE_DFOMT(n)		(0x3500 + ((n) << 2))
			/* Destination Address Filter Other Multicast Table */
#define MVXPE_NDFUT		4
#define MVXPE_DFUT(n)		(0x3600 + ((n) << 2))
			/* Destination Address Filter Unicast Table */

/* Rx DMA Miscellaneous Registers */
#define MVXPE_PMFS		0x247c	/* Port Rx Minimal Frame Size */
#define MVXPE_PDFC		0x2484	/* Port Rx Discard Frame Counter */
#define MVXPE_POFC		0x2488	/* Port Overrun Frame Counter */
#define MVXPE_RQC		0x2680	/* Receive Queue Command */

/* Rx DMA Networking Controller Miscellaneous Registers */
#define MVXPE_PRXC(q)		(0x1400 + ((q) << 2)) /*Port RX queues Config*/
#define MVXPE_PRXSNP(q)		(0x1420 + ((q) << 2)) /* Port RX queues Snoop */
#define MVXPE_PRXDQA(q)		(0x1480 + ((q) << 2)) /*P RXqueues desc Q Addr*/
#define MVXPE_PRXDQS(q)		(0x14a0 + ((q) << 2)) /*P RXqueues desc Q Size*/
#define MVXPE_PRXDQTH(q)	(0x14c0 + ((q) << 2)) /*P RXqueues desc Q Thrs*/
#define MVXPE_PRXS(q)		(0x14e0 + ((q) << 2)) /*Port RX queues Status */
#define MVXPE_PRXSU(q)		(0x1500 + ((q) << 2)) /*P RXqueues Stat Update*/
#define MVXPE_PRXDI(q)		(0x1520 + ((q) << 2)) /*P RXqueues Stat Update*/
#define MVXPE_PRXINIT		0x1cc0	/* Port RX Initialization */

/* Rx DMA Wake on LAN Registers	0x3690 - 0x36b8 */

/* Tx DMA Miscellaneous Registers */
#define MVXPE_TQC		0x2448	/* Transmit Queue Command */
#define MVXPE_PXTFTT		0x2478	/* Port Tx FIFO Threshold */
#define MVXPE_TXBADFCS		0x3cc0	/*Tx Bad FCS Transmitted Pckts Counter*/
#define MVXPE_TXDROPPED		0x3cc4	/* Tx Dropped Packets Counter */

/* Tx DMA Networking Controller Miscellaneous Registers */
#define MVXPE_PTXDQA(q)		(0x3c00 + ((q) << 2)) /*P TXqueues desc Q Addr*/
#define MVXPE_PTXDQS(q)		(0x3c20 + ((q) << 2)) /*P TXqueues desc Q Size*/
#define MVXPE_PTXS(q)		(0x3c40 + ((q) << 2)) /* Port TX queues Status*/
#define MVXPE_PTXSU(q)		(0x3c60 + ((q) << 2)) /*P TXqueues Stat Update*/
#define MVXPE_PTXDI(q)		(0x3c80 + ((q) << 2)) /* P TXqueues Desc Index*/
#define MVXPE_TXTBC(q)		(0x3ca0 + ((q) << 2)) /* TX Trans-ed Buf Count*/
#define MVXPE_PTXINIT		0x3cf0	/* Port TX Initialization */

/* Tx DMA Packet Modification Registers */
#define MVXPE_NMH		15
#define MVXPE_TXMH(n)		(0x3d44 + ((n) << 2))
#define MVXPE_TXMTU		0x3d88

/* Tx DMA Queue Arbiter Registers (Version 1) */
#define MVXPE_TQFPC_V1		0x24dc	/* Transmit Queue Fixed Priority Cfg */
#define MVXPE_TQTBC_V1		0x24e0	/* Transmit Queue Token-Bucket Cfg */
#define MVXPE_MTU_V1		0x24e8	/* MTU */
#define MVXPE_PMTBS_V1		0x24ec	/* Port Max Token-Bucket Size */
#define MVXPE_TQTBCOUNT_V1(q)	(0x2700 + ((q) << 4))
				/* Transmit Queue Token-Bucket Counter */
#define MVXPE_TQTBCONFIG_V1(q)	(0x2704 + ((q) << 4))
				/* Transmit Queue Token-Bucket Configuration */
#define MVXPE_PTTBC_V1		0x2740	/* Port Transmit Backet Counter */

/* Tx DMA Queue Arbiter Registers (Version 3) */
#define MVXPE_TQC1_V3		0x3e00	/* Transmit Queue Command1 */
#define MVXPE_TQFPC_V3		0x3e04	/* Transmit Queue Fixed Priority Cfg */
#define MVXPE_BRC_V3		0x3e08	/* Basic Refill No of Clocks */
#define MVXPE_MTU_V3		0x3e0c	/* MTU */
#define MVXPE_PREFILL_V3	0x3e10	/* Port Backet Refill */
#define MVXPE_PMTBS_V3		0x3e14	/* Port Max Token-Bucket Size */
#define MVXPE_QREFILL_V3(q)	(0x3e20 + ((q) << 2))
				/* Transmit Queue Refill */
#define MVXPE_QMTBS_V3(q)	(0x3e40 + ((q) << 2))
				/* Transmit Queue Max Token-Bucket Size */
#define MVXPE_QTTBC_V3(q)	(0x3e60 + ((q) << 2))
				/* Transmit Queue Token-Bucket Counter */
#define MVXPE_TQAC_V3(q)	(0x3e80 + ((q) << 2))
				/* Transmit Queue Arbiter Cfg */
#define MVXPE_TQIPG_V3(q)	(0x3ea0 + ((q) << 2))
				/* Transmit Queue IPG(valid q=2..3) */
#define MVXPE_HITKNINLOPKT_V3	0x3eb0	/* High Token in Low Packet */
#define MVXPE_HITKNINASYNCPKT_V3	0x3eb4	/* High Token in Async Packet */
#define MVXPE_LOTKNINASYNCPKT_V3	0x3eb8	/* Low Token in Async Packet */
#define MVXPE_TS_V3		0x3ebc	/* Token Speed */

/* RX_TX DMA Registers */
#define MVXPE_PXC		0x2400	/* Port Configuration */
#define MVXPE_PXCX		0x2404	/* Port Configuration Extend */
#define MVXPE_MH		0x2454	/* Marvell Header */

/* Serial(SMI/MII) Registers */
#define MVXPE_PSC0		0x243c	/* Port Serial Control0 */
#define MVXPE_PS0		0x2444	/* Ethernet Port Status */
#define MVXPE_PSERDESCFG	0x24a0	/* Serdes Configuration */
#define MVXPE_PSERDESSTS	0x24a4	/* Serdes Status */
#define MVXPE_PSOMSCD		0x24f4	/* One mS Clock Divider */
#define MVXPE_PSPFCCD		0x24f8	/* Periodic Flow Control Clock Divider*/

/* Gigabit Ethernet MAC Serial Parameters Configuration Registers */
#define MVXPE_PSPC		0x2c14	/* Port Serial Parameters Config */
#define MVXPE_PSP1C		0x2c94	/* Port Serial Parameters 1 Config */

/* Gigabit Ethernet Auto-Negotiation Configuration Registers */
#define MVXPE_PANC		0x2c0c	/* Port Auto-Negotiation Configuration*/

/* Gigabit Ethernet MAC Control Registers */
#define MVXPE_PMACC0		0x2c00	/* Port MAC Control 0 */
#define MVXPE_PMACC1		0x2c04	/* Port MAC Control 1 */
#define MVXPE_PMACC2		0x2c08	/* Port MAC Control 2 */
#define MVXPE_PMACC3		0x2c48	/* Port MAC Control 3 */
#define MVXPE_CCFCPST(p)	(0x2c58 + ((p) << 2)) /*CCFC Port Speed Timerp*/
#define MVXPE_PMACC4		0x2c90	/* Port MAC Control 4 */

/* Gigabit Ethernet MAC Interrupt Registers */
#define MVXPE_PIC		0x2c20
#define MVXPE_PIM		0x2c24

/* Gigabit Ethernet Low Power Idle  Registers */
#define MVXPE_LPIC0		0x2cc0	/* LowPowerIdle control 0 */
#define MVXPE_LPIC1		0x2cc4	/* LPI control 1 */
#define MVXPE_LPIC2		0x2cc8	/* LPI control 2 */
#define MVXPE_LPIS		0x2ccc	/* LPI status */
#define MVXPE_LPIC		0x2cd0	/* LPI counter */

/* Gigabit Ethernet MAC PRBS Check Status Registers */
#define MVXPE_PPRBSS		0x2c38	/* Port PRBS Status */
#define MVXPE_PPRBSEC		0x2c3c	/* Port PRBS Error Counter */

/* Gigabit Ethernet MAC Status Registers */
#define MVXPE_PSR		0x2c10	/* Port Status Register0 */

/* Networking Controller Interrupt Registers */
#define MVXPE_PRXITTH(q)	(0x2580 + ((q) << 2))
				/* Port Rx Interrupt Threshold */
#define MVXPE_PRXTXTIC		0x25a0	/*Port RX_TX Threshold Interrupt Cause*/
#define MVXPE_PRXTXTIM		0x25a4	/*Port RX_TX Threshold Interrupt Mask */
#define MVXPE_PRXTXIC		0x25a8	/* Port RX_TX Interrupt Cause */
#define MVXPE_PRXTXIM		0x25ac	/* Port RX_TX Interrupt Mask */
#define MVXPE_PMIC		0x25b0	/* Port Misc Interrupt Cause */
#define MVXPE_PMIM		0x25b4	/* Port Misc Interrupt Mask */
#define MVXPE_PIE		0x25b8	/* Port Interrupt Enable */

/* Miscellaneous Interrupt Registers */
#define MVXPE_PEUIAE		0x2494	/* Port Internal Address Error */

/* SGMII PHY Registers */
#define MVXPE_PPLLC		0x2e04	/* Power and PLL Control */
#define MVXPE_TESTC0		0x2e54	/* PHY Test Control 0 */
#define MVXPE_TESTPRBSEC0	0x2e7c	/* PHY Test PRBS Erorr Counter 0 */
#define MVXPE_TESTPRBSEC1	0x2e80	/* PHY Test PRBS Erorr Counter 1 */
#define MVXPE_TESTOOB0		0x2e84	/* PHY Test OOB 0 */
#define MVXPE_DLE		0x2e8c	/* Digital Loopback Enable */
#define MVXPE_RCS		0x2f18	/* Reference Clock Select */
#define MVXPE_COMPHYC		0x2f18	/* COMPHY Control */

/*
 * Ethernet MAC MIB Registers
 *  GbE0 BASE 0x00007.3000
 *  GbE1 BASE 0x00007.7000
 */
/* MAC MIB Counters			0x3000 - 0x307c */
#define MVXPE_PORTMIB_BASE		0x3000
#define MVXPE_PORTMIB_SIZE		0x0100

/* Rx */
#define MVXPE_MIB_RX_GOOD_OCT		0x00 /* 64bit */
#define MVXPE_MIB_RX_BAD_OCT		0x08
#define MVXPE_MIB_RX_MAC_TRNS_ERR	0x0c
#define MVXPE_MIB_RX_GOOD_FRAME		0x10
#define MVXPE_MIB_RX_BAD_FRAME		0x14
#define MVXPE_MIB_RX_BCAST_FRAME	0x18
#define MVXPE_MIB_RX_MCAST_FRAME	0x1c
#define MVXPE_MIB_RX_FRAME64_OCT	0x20
#define MVXPE_MIB_RX_FRAME127_OCT	0x24
#define MVXPE_MIB_RX_FRAME255_OCT	0x28
#define MVXPE_MIB_RX_FRAME511_OCT	0x2c
#define MVXPE_MIB_RX_FRAME1023_OCT	0x30
#define MVXPE_MIB_RX_FRAMEMAX_OCT	0x34

/* Tx */
#define MVXPE_MIB_TX_GOOD_OCT		0x38 /* 64bit */
#define MVXPE_MIB_TX_GOOD_FRAME		0x40
#define MVXPE_MIB_TX_EXCES_COL		0x44
#define MVXPE_MIB_TX_MCAST_FRAME	0x48
#define MVXPE_MIB_TX_BCAST_FRAME	0x4c
#define MVXPE_MIB_TX_MAC_CTL_ERR	0x50

/* Flow Control */
#define MVXPE_MIB_FC_SENT		0x54
#define MVXPE_MIB_FC_GOOD		0x58
#define MVXPE_MIB_FC_BAD		0x5c

/* Packet Processing */
#define MVXPE_MIB_PKT_UNDERSIZE		0x60
#define MVXPE_MIB_PKT_FRAGMENT		0x64
#define MVXPE_MIB_PKT_OVERSIZE		0x68
#define MVXPE_MIB_PKT_JABBER		0x6c

/* MAC Layer Errors */
#define MVXPE_MIB_MAC_RX_ERR		0x70
#define MVXPE_MIB_MAC_CRC_ERR		0x74
#define MVXPE_MIB_MAC_COL		0x78
#define MVXPE_MIB_MAC_LATE_COL		0x7c

/* END OF REGISTER NUMBERS */

/*
 *
 * Register Formats
 *
 */
/*
 * Address Decoder Registers
 */
/* Base Address (MVXPE_BASEADDR) */
#define MVXPE_BASEADDR_TARGET(target)	((target) & 0xf)
#define MVXPE_BASEADDR_ATTR(attr)	(((attr) & 0xff) << 8)
#define MVXPE_BASEADDR_BASE(base)	((base) & 0xffff0000)

/* Size (MVXPE_S) */
#define MVXPE_S_SIZE(size)		(((size) - 1) & 0xffff0000)

/* Base Address Enable (MVXPE_BARE) */
#define MVXPE_BARE_EN_MASK		((1 << MVXPE_NWINDOW) - 1)
#define MVXPE_BARE_EN(win)		((1 << (win)) & MVXPE_BARE_EN_MASK)

/* Ethernet Port Access Protect (MVXPE_EPAP) */
#define MVXPE_EPAP_AC_NAC		0x0	/* No access allowed */
#define MVXPE_EPAP_AC_RO		0x1	/* Read Only */
#define MVXPE_EPAP_AC_FA		0x3	/* Full access (r/w) */
#define MVXPE_EPAP_EPAR(win, ac)	((ac) << ((win) * 2))

/*
 * Global Miscellaneous Registers
 */
/* PHY Address (MVXPE_PHYADDR) */
#define MVXPE_PHYADDR_PHYAD(phy)	((phy) & 0x1f)
#define MVXPE_PHYADDR_GET_PHYAD(reg)	((reg) & 0x1f)

/* SMI register fields (MVXPE_SMI) */
#define MVXPE_SMI_DATA_MASK		0x0000ffff
#define MVXPE_SMI_PHYAD(phy)		(((phy) & 0x1f) << 16)
#define MVXPE_SMI_REGAD(reg)		(((reg) & 0x1f) << 21)
#define MVXPE_SMI_OPCODE_WRITE		(0 << 26)
#define MVXPE_SMI_OPCODE_READ		(1 << 26)
#define MVXPE_SMI_READVALID		(1 << 27)
#define MVXPE_SMI_BUSY			(1 << 28)

/* Ethernet Unit Default ID (MVXPE_EUDID) */
#define MVXPE_EUDID_DIDR_MASK		0x0000000f
#define MVXPE_EUDID_DIDR(id)		((id) & 0x0f)
#define MVXPE_EUDID_DATTR_MASK		0x00000ff0
#define MVXPE_EUDID_DATTR(attr)		(((attr) & 0xff) << 4)

/* Ethernet Unit Interrupt Cause (MVXPE_EUIC) */
#define MVXPE_EUIC_ETHERINTSUM 		(1 << 0)
#define MVXPE_EUIC_PARITY 		(1 << 1)
#define MVXPE_EUIC_ADDRVIOL		(1 << 2)
#define MVXPE_EUIC_ADDRVNOMATCH		(1 << 3)
#define MVXPE_EUIC_SMIDONE		(1 << 4)
#define MVXPE_EUIC_COUNTWA		(1 << 5)
#define MVXPE_EUIC_INTADDRERR		(1 << 7)
#define MVXPE_EUIC_PORT0DPERR		(1 << 9)
#define MVXPE_EUIC_TOPDPERR		(1 << 12)

/* Ethernet Unit Internal Addr Error (MVXPE_EUIAE) */
#define MVXPE_EUIAE_INTADDR_MASK 	0x000001ff
#define MVXPE_EUIAE_INTADDR(addr)	((addr) & 0x1ff)
#define MVXPE_EUIAE_GET_INTADDR(addr)	((addr) & 0x1ff)

/* Ethernet Unit Control (MVXPE_EUC) */
#define MVXPE_EUC_POLLING	 	(1 << 1)
#define MVXPE_EUC_PORTRESET	 	(1 << 24)
#define MVXPE_EUC_RAMSINITIALIZATIONCOMPLETED (1 << 25)

/*
 * Miscellaneous Registers
 */
/* SDMA Configuration (MVXPE_SDC) */
#define MVXPE_SDC_RXBSZ(x)		((x) << 1)
#define MVXPE_SDC_RXBSZ_MASK		MVXPE_SDC_RXBSZ(7)
#define MVXPE_SDC_RXBSZ_1_64BITWORDS	MVXPE_SDC_RXBSZ(0)
#define MVXPE_SDC_RXBSZ_2_64BITWORDS	MVXPE_SDC_RXBSZ(1)
#define MVXPE_SDC_RXBSZ_4_64BITWORDS	MVXPE_SDC_RXBSZ(2)
#define MVXPE_SDC_RXBSZ_8_64BITWORDS	MVXPE_SDC_RXBSZ(3)
#define MVXPE_SDC_RXBSZ_16_64BITWORDS	MVXPE_SDC_RXBSZ(4)
#define MVXPE_SDC_BLMR			(1 << 4)
#define MVXPE_SDC_BLMT			(1 << 5)
#define MVXPE_SDC_SWAPMODE		(1 << 6)
#define MVXPE_SDC_TXBSZ(x)		((x) << 22)
#define MVXPE_SDC_TXBSZ_MASK		MVXPE_SDC_TXBSZ(7)
#define MVXPE_SDC_TXBSZ_1_64BITWORDS	MVXPE_SDC_TXBSZ(0)
#define MVXPE_SDC_TXBSZ_2_64BITWORDS	MVXPE_SDC_TXBSZ(1)
#define MVXPE_SDC_TXBSZ_4_64BITWORDS	MVXPE_SDC_TXBSZ(2)
#define MVXPE_SDC_TXBSZ_8_64BITWORDS	MVXPE_SDC_TXBSZ(3)
#define MVXPE_SDC_TXBSZ_16_64BITWORDS	MVXPE_SDC_TXBSZ(4)

/*
 * Networking Controller Miscellaneous Registers
 */
/* Port Acceleration Mode (MVXPE_PACC) */
#define MVXPE_PACC_ACCELERATIONMODE_MASK	0x7
#define MVXPE_PACC_ACCELERATIONMODE_EDM		0x1	/* Enhanced Desc Mode */

/* Port Version (MVXPE_PV) */
#define MVXPE_PV_VERSION_MASK			0xff
#define MVXPE_PV_VERSION(v)			((v) & 0xff)
#define MVXPE_PV_GET_VERSION(reg)		((reg) & 0xff)

/*
 * Rx DMA Hardware Parser Registers
 */
/* Ether Type Priority (MVXPE_ETP) */
#define MVXPE_ETP_ETHERTYPEPRIEN	(1 << 0)	/* EtherType Prio Ena */
#define MVXPE_ETP_ETHERTYPEPRIFRSTEN	(1 << 1)
#define MVXPE_ETP_ETHERTYPEPRIQ		(0x7 << 2)	/*EtherType Prio Queue*/
#define MVXPE_ETP_ETHERTYPEPRIVAL	(0xffff << 5)	/*EtherType Prio Value*/
#define MVXPE_ETP_FORCEUNICSTHIT	(1 << 21)	/* Force Unicast hit */

/* Destination Address Filter Registers (MVXPE_DF{SM,OM,U}T) */
#define MVXPE_DF(n, x)			((x) << (8 * (n)))
#define MVXPE_DF_PASS			(1 << 0)
#define MVXPE_DF_QUEUE(q)		((q) << 1)
#define MVXPE_DF_QUEUE_ALL		((7) << 1)
#define MVXPE_DF_QUEUE_MASK		((7) << 1)

/*
 * Rx DMA Miscellaneous Registers
 */
/* Port Rx Minimal Frame Size (MVXPE_PMFS) */
#define MVXPE_PMFS_RXMFS(rxmfs)		(((rxmfs) - 40) & 0x7c)

/* Receive Queue Command (MVXPE_RQC) */
#define MVXPE_RQC_EN_MASK		(0xff << 0)	/* Enable Q */
#define MVXPE_RQC_ENQ(q)		(1 << (0 + (q)))
#define MVXPE_RQC_EN(n)			((n) << 0)
#define MVXPE_RQC_DIS_MASK		(0xff << 8)	/* Disable Q */
#define MVXPE_RQC_DISQ(q)		(1 << (8 + (n)))
#define MVXPE_RQC_DIS(n)		((n) << 8)

/*
 * Rx DMA Networking Controller Miscellaneous Registers
 */
/* Port RX queues Configuration (MVXPE_PRXC) */
#define MVXPE_PRXC_PACKETOFFSET(o)	(((o) & 0xf) << 8)

/* Port RX queues Snoop (MVXPE_PRXSNP) */
#define MVXPE_PRXSNP_SNOOPNOOFBYTES(b)	(((b) & 0x3fff) << 0)
#define MVXPE_PRXSNP_L2DEPOSITNOOFBYTES(b) (((b) & 0x3fff) << 16)

/* Port RX queues Descriptors Queue Size (MVXPE_PRXDQS) */
#define MVXPE_PRXDQS_DESCRIPTORSQUEUESIZE(s)	(((s) & 0x3fff) << 0)
#define MVXPE_PRXDQS_BUFFERSIZE(s)		(((s) & 0x1fff) << 19)

/* Port RX queues Descriptors Queue Threshold (MVXPE_PRXDQTH) */
					/* Occupied Descriptors Threshold */
#define MVXPE_PRXDQTH_ODT(x)		(((x) & 0x3fff) << 0)
					/* Non Occupied Descriptors Threshold */
#define MVXPE_PRXDQTH_NODT(x)		(((x) & 0x3fff) << 16)

/* Port RX queues Status (MVXPE_PRXS) */
					/* Occupied Descriptors Counter */
#define MVXPE_PRXS_ODC(x)		(((x) & 0x3fff) << 0)
					/* Non Occupied Descriptors Counter */
#define MVXPE_PRXS_NODC(x)		(((x) & 0x3fff) << 16)
#define MVXPE_PRXS_GET_ODC(reg)		(((reg) >> 0) & 0x3fff)
#define MVXPE_PRXS_GET_NODC(reg)	(((reg) >> 16) & 0x3fff)

/* Port RX queues Status Update (MVXPE_PRXSU) */
#define MVXPE_PRXSU_NOOFPROCESSEDDESCRIPTORS(x) (((x) & 0xff) << 0)
#define MVXPE_PRXSU_NOOFNEWDESCRIPTORS(x) (((x) & 0xff) << 16)

/* Port RX Initialization (MVXPE_PRXINIT) */
#define MVXPE_PRXINIT_RXDMAINIT		(1 << 0)

/*
 * Rx DMA Wake on LAN Registers
 */
/* XXX: not implemented yet */

/*
 * Tx DMA Miscellaneous Registers
 */
/* Transmit Queue Command (MVXPE_TQC) */
#define MVXPE_TQC_EN_MASK		(0xff << 0)
#define MVXPE_TQC_ENQ(q)		(1 << ((q) + 0))/* Enable Q */
#define MVXPE_TQC_EN(n)			((n) << 0)
#define MVXPE_TQC_DIS_MASK		(0xff << 8)
#define MVXPE_TQC_DISQ(q)		(1 << ((q) + 8))/* Disable Q */
#define MVXPE_TQC_DIS(n)		((n) << 8)

/*
 * Tx DMA Networking Controller Miscellaneous Registers
 */
/* Port TX queues Descriptors Queue Size (MVXPE_PTXDQS) */
					/* Descriptors Queue Size */
#define MVXPE_PTXDQS_DQS_MASK		(0x3fff << 0)
#define MVXPE_PTXDQS_DQS(x)		(((x) & 0x3fff) << 0)
					/* Transmitted Buffer Threshold */
#define MVXPE_PTXDQS_TBT_MASK		(0x3fff << 16)
#define MVXPE_PTXDQS_TBT(x)		(((x) & 0x3fff) << 16)

/* Port TX queues Status (MVXPE_PTXS) */
					/* Transmitted Buffer Counter */
#define MVXPE_PTXS_TBC(x)		(((x) & 0x3fff) << 16)

#define MVXPE_PTXS_GET_TBC(reg)		(((reg) >> 16) & 0x3fff)
					/* Pending Descriptors Counter */
#define MVXPE_PTXS_PDC(x)		((x) & 0x3fff)
#define MVXPE_PTXS_GET_PDC(x)		((x) & 0x3fff)

/* Port TX queues Status Update (MVXPE_PTXSU) */
					/* Number Of Written Descriptoes */
#define MVXPE_PTXSU_NOWD(x)		(((x) & 0xff) << 0)
					/* Number Of Released Buffers */
#define MVXPE_PTXSU_NORB(x)		(((x) & 0xff) << 16)

/* TX Transmitted Buffers Counter (MVXPE_TXTBC) */
					/* Transmitted Buffers Counter */
#define MVXPE_TXTBC_TBC(x)		(((x) & 0x3fff) << 16)

/* Port TX Initialization (MVXPE_PTXINIT) */
#define MVXPE_PTXINIT_TXDMAINIT		(1 << 0)

/*
 * Tx DMA Packet Modification Registers
 */
/* XXX: not implemeted yet */

/*
 * Tx DMA Queue Arbiter Registers (Version 1 )
 */
/* XXX: not implemented yet */
/* Transmit Queue Fixed Priority Configuration */
#define MVXPE_TQFPC_EN(q)		(1 << (q))


/*
 * RX_TX DMA Registers
 */
/* Port Configuration (MVXPE_PXC) */
#define MVXPE_PXC_UPM			(1 << 0) /* Uni Promisc mode */
#define MVXPE_PXC_RXQ(q)		((q) << 1)
#define MVXPE_PXC_RXQ_MASK		MVXPE_PXC_RXQ(7)
#define MVXPE_PXC_RXQARP(q)		((q) << 4)
#define MVXPE_PXC_RXQARP_MASK		MVXPE_PXC_RXQARP(7)
#define MVXPE_PXC_RB			(1 << 7) /* Rej mode of MAC */
#define MVXPE_PXC_RBIP			(1 << 8)
#define MVXPE_PXC_RBARP			(1 << 9)
#define MVXPE_PXC_AMNOTXES		(1 << 12)
#define MVXPE_PXC_RBARPF		(1 << 13)
#define MVXPE_PXC_TCPCAPEN		(1 << 14)
#define MVXPE_PXC_UDPCAPEN		(1 << 15)
#define MVXPE_PXC_TCPQ(q)		((q) << 16)
#define MVXPE_PXC_TCPQ_MASK		MVXPE_PXC_TCPQ(7)
#define MVXPE_PXC_UDPQ(q)		((q) << 19)
#define MVXPE_PXC_UDPQ_MASK		MVXPE_PXC_UDPQ(7)
#define MVXPE_PXC_BPDUQ(q)		((q) << 22)
#define MVXPE_PXC_BPDUQ_MASK		MVXPE_PXC_BPDUQ(7)
#define MVXPE_PXC_RXCS			(1 << 25)

/* Port Configuration Extend (MVXPE_PXCX) */
#define MVXPE_PXCX_SPAN			(1 << 1)
#define MVXPE_PXCX_TXCRCDIS		(1 << 3)

/* Marvell Header (MVXPE_MH) */
#define MVXPE_MH_MHEN			(1 << 0)
#define MVXPE_MH_DAPREFIX		(0x3 << 1)
#define MVXPE_MH_SPID			(0xf << 4)
#define MVXPE_MH_MHMASK			(0x3 << 8)
#define MVXPE_MH_MHMASK_8QUEUES		(0x0 << 8)
#define MVXPE_MH_MHMASK_4QUEUES		(0x1 << 8)
#define MVXPE_MH_MHMASK_2QUEUES		(0x3 << 8)
#define MVXPE_MH_DSAEN_MASK		(0x3 << 10)
#define MVXPE_MH_DSAEN_DISABLE		(0x0 << 10)
#define MVXPE_MH_DSAEN_NONEXTENDED	(0x1 << 10)
#define MVXPE_MH_DSAEN_EXTENDED		(0x2 << 10)

/*
 * Serial(SMI/MII) Registers
 */
/* Port Seiral Control0 (MVXPE_PSC0) */
#define MVXPE_PSC0_FORCE_FC_MASK	(0x3 << 5)
#define MVXPE_PSC0_FORCE_FC(fc)		(((fc) & 0x3) << 5)
#define MVXPE_PSC0_FORCE_FC_PAUSE	MVXPE_PSC0_FORCE_FC(0x1)
#define MVXPE_PSC0_FORCE_FC_NO_PAUSE	MVXPE_PSC0_FORCE_FC(0x0)
#define MVXPE_PSC0_FORCE_BP_MASK	(0x3 << 7)
#define MVXPE_PSC0_FORCE_BP(fc)		(((fc) & 0x3) << 5)
#define MVXPE_PSC0_FORCE_BP_JAM		MVXPE_PSC0_FORCE_BP(0x1)
#define MVXPE_PSC0_FORCE_BP_NO_JAM	MVXPE_PSC0_FORCE_BP(0x0)
#define MVXPE_PSC0_DTE_ADV		(1 << 14)
#define MVXPE_PSC0_IGN_RXERR		(1 << 28)
#define MVXPE_PSC0_IGN_COLLISION	(1 << 29)
#define MVXPE_PSC0_IGN_CARRIER		(1 << 30)

/* Ethernet Port Status0 (MVXPE_PS0) */
#define MVXPE_PS0_TXINPROG		(1 << 0)
#define MVXPE_PS0_TXFIFOEMP		(1 << 8)
#define MVXPE_PS0_RXFIFOEMPTY		(1 << 16)

/*
 * Gigabit Ethernet MAC Serial Parameters Configuration Registers
 */
#define MVXPE_PSPC_MUST_SET		(1 << 3 | 1 << 4 | 1 << 5 | 0x23 << 6)
#define MVXPE_PSP1C_MUST_SET		(1 << 0 | 1 << 1 | 1 << 2)

/*
 * Gigabit Ethernet Auto-Negotiation Configuration Registers
 */
/* Port Auto-Negotiation Configuration (MVXPE_PANC) */
#define MVXPE_PANC_FORCELINKFAIL	(1 << 0)
#define MVXPE_PANC_FORCELINKPASS	(1 << 1)
#define MVXPE_PANC_INBANDANEN		(1 << 2)
#define MVXPE_PANC_INBANDANBYPASSEN	(1 << 3)
#define MVXPE_PANC_INBANDRESTARTAN	(1 << 4)
#define MVXPE_PANC_SETMIISPEED		(1 << 5)
#define MVXPE_PANC_SETGMIISPEED		(1 << 6)
#define MVXPE_PANC_ANSPEEDEN		(1 << 7)
#define MVXPE_PANC_SETFCEN		(1 << 8)
#define MVXPE_PANC_PAUSEADV		(1 << 9)
#define MVXPE_PANC_ANFCEN		(1 << 11)
#define MVXPE_PANC_SETFULLDX		(1 << 12)
#define MVXPE_PANC_ANDUPLEXEN		(1 << 13)
#define MVXPE_PANC_MUSTSET		(1 << 15)

/*
 * Gigabit Ethernet MAC Control Registers
 */
/* Port MAC Control 0 (MVXPE_PMACC0) */
#define MVXPE_PMACC0_PORTEN		(1 << 0)
#define MVXPE_PMACC0_PORTTYPE		(1 << 1)
#define MVXPE_PMACC0_FRAMESIZELIMIT(x)	((((x) >> 1) & 0x7ffc) << 2)
#define MVXPE_PMACC0_MUSTSET		(1 << 15)

/* Port MAC Control 1 (MVXPE_PMACC1) */
#define MVXPE_PMACC1_PCSLB		(1 << 6)

/* Port MAC Control 2 (MVXPE_PMACC2) */
#define MVXPE_PMACC2_PCSEN		(1 << 3)
#define MVXPE_PMACC2_RGMIIEN		(1 << 4)
#define MVXPE_PMACC2_PADDINGDIS		(1 << 5)
#define MVXPE_PMACC2_PORTMACRESET	(1 << 6)
#define MVXPE_PMACC2_PRBSCHECKEN	(1 << 10)
#define MVXPE_PMACC2_PRBSGENEN		(1 << 11)
#define MVXPE_PMACC2_SDTT_MASK		(3 << 12)  /* Select Data To Transmit */
#define MVXPE_PMACC2_SDTT_RM		(0 << 12)	/* Regular Mode */
#define MVXPE_PMACC2_SDTT_PRBS		(1 << 12)	/* PRBS Mode */
#define MVXPE_PMACC2_SDTT_ZC		(2 << 12)	/* Zero Constant */
#define MVXPE_PMACC2_SDTT_OC		(3 << 12)	/* One Constant */
#define MVXPE_PMACC2_MUSTSET		(3 << 14)

/* Port MAC Control 3 (MVXPE_PMACC3) */
#define MVXPE_PMACC3_IPG_MASK		0x7f80

/*
 * Gigabit Ethernet MAC Interrupt Registers
 */
/* Port Interrupt Cause/Mask (MVXPE_PIC/MVXPE_PIM) */
#define MVXPE_PI_INTSUM			(1 << 0)
#define MVXPE_PI_LSC			(1 << 1)   /* LinkStatus Change */
#define MVXPE_PI_ACOP			(1 << 2)   /* AnCompleted OnPort */
#define MVXPE_PI_AOOR			(1 << 5)   /* AddressOut Of Range */
#define MVXPE_PI_SSC			(1 << 6)   /* SyncStatus Change */
#define MVXPE_PI_PRBSEOP		(1 << 7)   /* QSGMII PRBS error */
#define MVXPE_PI_MIBCWA			(1 << 15)  /* MIB counter wrap around */
#define MVXPE_PI_QSGMIIPRBSE		(1 << 10)  /* QSGMII PRBS error */
#define MVXPE_PI_PCSRXPRLPI		(1 << 11)  /* PCS Rx path received LPI*/
#define MVXPE_PI_PCSTXPRLPI		(1 << 12)  /* PCS Tx path received LPI*/
#define MVXPE_PI_MACRXPRLPI		(1 << 13)  /* MAC Rx path received LPI*/
#define MVXPE_PI_MIBCCD			(1 << 14)  /* MIB counters copy done */

/*
 * Gigabit Ethernet MAC Low Power Idle Registers
 */
/* LPI Control 0 (MVXPE_LPIC0) */
#define MVXPE_LPIC0_LILIMIT(x)		(((x) & 0xff) << 0)
#define MVXPE_LPIC0_TSLIMIT(x)		(((x) & 0xff) << 8)

/* LPI Control 1 (MVXPE_LPIC1) */
#define MVXPE_LPIC1_LPIRE		(1 << 0)	/* LPI request enable */
#define MVXPE_LPIC1_LPIRF		(1 << 1)	/* LPI request force */
#define MVXPE_LPIC1_LPIMM		(1 << 2)	/* LPI manual mode */
#define MVXPE_LPIC1_TWLIMIT(x)		(((x) & 0xfff) << 4)

/* LPI Control 2 (MVXPE_LPIC2) */
#define MVXPE_LPIC2_MUSTSET		0x17d

/* LPI Status (MVXPE_LPIS) */
#define MVXPE_LPIS_PCSRXPLPIS		(1 << 0) /* PCS Rx path LPI status */
#define MVXPE_LPIS_PCSTXPLPIS		(1 << 1) /* PCS Tx path LPI status */
#define MVXPE_LPIS_MACRXPLPIS		(1 << 2)/* MAC Rx path LP idle status */
#define MVXPE_LPIS_MACTXPLPWS		(1 << 3)/* MAC Tx path LP wait status */
#define MVXPE_LPIS_MACTXPLPIS		(1 << 4)/* MAC Tx path LP idle status */

/*
 * Gigabit Ethernet MAC PRBS Check Status Registers
 */
/* Port PRBS Status (MVXPE_PPRBSS) */
#define MVXPE_PPRBSS_PRBSCHECKLOCKED	(1 << 0)
#define MVXPE_PPRBSS_PRBSCHECKRDY	(1 << 1)

/*
 * Gigabit Ethernet MAC Status Registers
 */
/* Port Status Register (MVXPE_PSR) */
#define MVXPE_PSR_LINKUP		(1 << 0)
#define MVXPE_PSR_GMIISPEED		(1 << 1)
#define MVXPE_PSR_MIISPEED		(1 << 2)
#define MVXPE_PSR_FULLDX		(1 << 3)
#define MVXPE_PSR_RXFCEN		(1 << 4)
#define MVXPE_PSR_TXFCEN		(1 << 5)
#define MVXPE_PSR_PRP			(1 << 6) /* Port Rx Pause */
#define MVXPE_PSR_PTP			(1 << 7) /* Port Tx Pause */
#define MVXPE_PSR_PDP			(1 << 8) /*Port is Doing Back-Pressure*/
#define MVXPE_PSR_SYNCFAIL10MS		(1 << 10)
#define MVXPE_PSR_ANDONE		(1 << 11)
#define MVXPE_PSR_IBANBA		(1 << 12) /* InBand AutoNeg BypassAct */
#define MVXPE_PSR_SYNCOK		(1 << 14)

/*
 * Networking Controller Interrupt Registers
 */
/* Port RX_TX Interrupt Threshold */
#define MVXPE_PRXITTH_RITT(t)		((t) & 0xffffff)

/* Port RX_TX Threshold Interrupt Cause/Mask (MVXPE_PRXTXTIC/MVXPE_PRXTXTIM) */
#define MVXPE_PRXTXTI_TBTCQ(q)		(1 << ((q) + 0))
#define MVXPE_PRXTXTI_TBTCQ_MASK	(0xff << 0)
#define MVXPE_PRXTXTI_GET_TBTCQ(reg)	(((reg) >> 0) & 0xff)
					/* Tx Buffer Threshold Cross Queue*/
#define MVXPE_PRXTXTI_RBICTAPQ(q)	(1 << ((q) + 8))
#define MVXPE_PRXTXTI_RBICTAPQ_MASK	(0xff << 8)
#define MVXPE_PRXTXTI_GET_RBICTAPQ(reg)	(((reg) >> 8) & 0xff)
				/* Rx Buffer Int. Coaleasing Th. Pri. Alrt Q */
#define MVXPE_PRXTXTI_RDTAQ(q)		(1 << ((q) + 16))
#define MVXPE_PRXTXTI_RDTAQ_MASK	(0xff << 16)
#define MVXPE_PRXTXTI_GET_RDTAQ(reg)	(((reg) >> 16) & 0xff)
					/* Rx Descriptor Threshold Alert Queue*/
#define MVXPE_PRXTXTI_PRXTXICSUMMARY	(1 << 29)	/* PRXTXI summary */
#define MVXPE_PRXTXTI_PTXERRORSUMMARY	(1 << 30)	/* PTEXERROR summary */
#define MVXPE_PRXTXTI_PMISCICSUMMARY	(1 << 31)	/* PMISCIC summary */

/* Port RX_TX Interrupt Cause/Mask (MVXPE_PRXTXIC/MVXPE_PRXTXIM) */
#define MVXPE_PRXTXI_TBRQ(q)		(1 << ((q) + 0))
#define MVXPE_PRXTXI_TBRQ_MASK		(0xff << 0)
#define MVXPE_PRXTXI_GET_TBRQ(reg)	(((reg) >> 0) & 0xff)
#define MVXPE_PRXTXI_RPQ(q)		(1 << ((q) + 8))
#define MVXPE_PRXTXI_RPQ_MASK		(0xff << 8)
#define MVXPE_PRXTXI_GET_RPQ(reg)	(((reg) >> 8) & 0xff)
#define MVXPE_PRXTXI_RREQ(q)		(1 << ((q) + 16))
#define MVXPE_PRXTXI_RREQ_MASK		(0xff << 16)
#define MVXPE_PRXTXI_GET_RREQ(reg)	(((reg) >> 16) & 0xff)
#define MVXPE_PRXTXI_PRXTXTHICSUMMARY	(1 << 29)
#define MVXPE_PRXTXI_PTXERRORSUMMARY	(1 << 30)
#define MVXPE_PRXTXI_PMISCICSUMMARY	(1 << 31)

/* Port Misc Interrupt Cause/Mask (MVXPE_PMIC/MVXPE_PMIM) */
#define MVXPE_PMI_PHYSTATUSCHNG		(1 << 0)
#define MVXPE_PMI_LINKCHANGE		(1 << 1)
#define MVXPE_PMI_IAE			(1 << 7) /* Internal Address Error */
#define MVXPE_PMI_RXOVERRUN		(1 << 8)
#define MVXPE_PMI_RXCRCERROR		(1 << 9)
#define MVXPE_PMI_RXLARGEPACKET		(1 << 10)
#define MVXPE_PMI_TXUNDRN		(1 << 11)
#define MVXPE_PMI_PRBSERROR		(1 << 12)
#define MVXPE_PMI_SRSE			(1 << 14) /* SerdesRealignSyncError */
#define MVXPE_PMI_TREQ(q)		(1 << ((q) + 24)) /* TxResourceErrorQ */
#define MVXPE_PMI_TREQ_MASK		(0xff << 24) /* TxResourceErrorQ */

/* Port Interrupt Enable (MVXPE_PIE) */
#define MVXPE_PIE_RXPKTINTRPTENB(q)	(1 << ((q) + 0))
#define MVXPE_PIE_TXPKTINTRPTENB(q)	(1 << ((q) + 8))
#define MVXPE_PIE_RXPKTINTRPTENB_MASK	(0xff << 0)
#define MVXPE_PIE_TXPKTINTRPTENB_MASK	(0xff << 8)

/*
 * Miscellaneous Interrupt Registers
 */
#define MVXPE_PEUIAE_ADDR_MASK		(0x3fff)
#define MVXPE_PEUIAE_ADDR(addr)		((addr) & 0x3fff)
#define MVXPE_PEUIAE_GET_ADDR(reg)	((reg) & 0x3fff)

/*
 * SGMII PHY Registers
 */
/* Power and PLL Control (MVXPE_PPLLC) */
#define MVXPE_PPLLC_REF_FREF_SEL_MASK	(0xf << 0)
#define MVXPE_PPLLC_PHY_MODE_MASK	(7 << 5)
#define MVXPE_PPLLC_PHY_MODE_SATA	(0 << 5)
#define MVXPE_PPLLC_PHY_MODE_SAS	(1 << 5)
#define MVXPE_PPLLC_PLL_LOCK		(1 << 8)
#define MVXPE_PPLLC_PU_DFE		(1 << 10)
#define MVXPE_PPLLC_PU_TX_INTP		(1 << 11)
#define MVXPE_PPLLC_PU_TX		(1 << 12)
#define MVXPE_PPLLC_PU_RX		(1 << 13)
#define MVXPE_PPLLC_PU_PLL		(1 << 14)

/* Digital Loopback Enable (MVXPE_DLE) */
#define MVXPE_DLE_LOCAL_SEL_BITS_MASK	(3 << 10)
#define MVXPE_DLE_LOCAL_SEL_BITS_10BITS	(0 << 10)
#define MVXPE_DLE_LOCAL_SEL_BITS_20BITS	(1 << 10)
#define MVXPE_DLE_LOCAL_SEL_BITS_40BITS	(2 << 10)
#define MVXPE_DLE_LOCAL_RXPHER_TO_TX_EN	(1 << 12)
#define MVXPE_DLE_LOCAL_ANA_TX2RX_LPBK_EN (1 << 13)
#define MVXPE_DLE_LOCAL_DIG_TX2RX_LPBK_EN (1 << 14)
#define MVXPE_DLE_LOCAL_DIG_RX2TX_LPBK_EN (1 << 15)

/* Reference Clock Select (MVXPE_RCS) */
#define MVXPE_RCS_REFCLK_SEL		(1 << 10)

/*
 * DMA descriptors
 */
struct mvxpe_tx_desc {
	/* LITTLE_ENDIAN */
	uint32_t command;		/* off 0x00: commands */
	uint16_t l4ichk;		/* initial checksum */
	uint16_t bytecnt;		/* 0ff 0x04: buffer byte count */
	uint32_t bufptr;		/* off 0x08: buffer ptr(PA) */
	uint32_t flags;			/* off 0x0c: flags */
	uint32_t reserved0;		/* off 0x10 */
	uint32_t reserved1;		/* off 0x14 */
	uint32_t reserved2;		/* off 0x18 */
	uint32_t reserved3;		/* off 0x1c */
};

struct mvxpe_rx_desc {
	/* LITTLE_ENDIAN */
	uint32_t status;		/* status and flags */
	uint16_t reserved0;
	uint16_t bytecnt;		/* buffer byte count */
	uint32_t bufptr;		/* packet buffer pointer */
	uint32_t reserved1;
	uint32_t reserved2;
	uint16_t reserved3;
	uint16_t l4chk;			/* L4 checksum */
	uint32_t reserved4;
	uint32_t reserved5;
};

/*
 * Received pakcet command header:
 *  network controller => software
 * the controller parse the packet and set some flags.
 */
#define MVXPE_RX_IPV4_FRAGMENT	(1 << 31) /* Fragment Indicator */
#define MVXPE_RX_L4_CHECKSUM_OK	(1 << 30) /* L4 Checksum */
/* bit 29 reserved */
#define MVXPE_RX_U			(1 << 28) /* Unknown Destination */
#define MVXPE_RX_F			(1 << 27) /* First buffer */
#define MVXPE_RX_L			(1 << 26) /* Last buffer */
#define MVXPE_RX_IP_HEADER_OK		(1 << 25) /* IP Header is OK */
#define MVXPE_RX_L3_IP			(1 << 24) /* IP Type 0:IP6 1:IP4 */
#define MVXPE_RX_L2_EV2			(1 << 23) /* Ethernet v2 frame */
#define MVXPE_RX_L4_MASK		(3 << 21) /* L4 Type */
#define MVXPE_RX_L4_TCP			(0x00 << 21)
#define MVXPE_RX_L4_UDP			(0x01 << 21)
#define MVXPE_RX_L4_OTH			(0x10 << 21)
#define MVXPE_RX_BPDU			(1 << 20) /* BPDU frame */
#define MVXPE_RX_VLAN			(1 << 19) /* VLAN tag found */
#define MVXPE_RX_EC_MASK		(3 << 17) /* Error code */
#define MVXPE_RX_EC_CE			(0x00 << 17) /* CRC error */
#define MVXPE_RX_EC_OR			(0x01 << 17) /* FIFO overrun */
#define MVXPE_RX_EC_MF			(0x10 << 17) /* Max. frame len */
#define MVXPE_RX_EC_RE			(0x11 << 17) /* Resource error */
#define MVXPE_RX_ES			(1 << 16) /* Error summary */
/* bit 15:0 reserved */

/*
 * Transmit packet command header:
 *  software => network controller
 */
#define MVXPE_TX_CMD_L4_CHECKSUM_MASK	(0x3 << 30) /* Do L4 Checksum */
#define MVXPE_TX_CMD_L4_CHECKSUM_FRAG	(0x0 << 30)
#define MVXPE_TX_CMD_L4_CHECKSUM_NOFRAG	(0x1 << 30)
#define MVXPE_TX_CMD_L4_CHECKSUM_NONE	(0x2 << 30)
#define MVXPE_TX_CMD_PACKET_OFFSET_MASK	(0x7f << 23) /* Payload offset */
#define MVXPE_TX_CMD_W_PACKET_OFFSET(v)	(((v) & 0x7f) << 23)
/* bit 22 reserved */
#define MVXPE_TX_CMD_F			(1 << 21) /* First buffer */
#define MVXPE_TX_CMD_L			(1 << 20) /* Last buffer */
#define MVXPE_TX_CMD_PADDING		(1 << 19) /* Pad short frame */
#define MVXPE_TX_CMD_IP4_CHECKSUM	(1 << 18) /* Do IPv4 Checksum */
#define MVXPE_TX_CMD_L3_IP4		(0 << 17)
#define MVXPE_TX_CMD_L3_IP6		(1 << 17)
#define MVXPE_TX_CMD_L4_TCP		(0 << 16)
#define MVXPE_TX_CMD_L4_UDP		(1 << 16)
/* bit 15:13 reserved */
#define MVXPE_TX_CMD_IP_HEADER_LEN_MASK	(0x1f << 8) /* IP header len >> 2 */
#define MVXPE_TX_CMD_IP_HEADER_LEN(v)	(((v) & 0x1f) << 8)
/* bit 7 reserved */
#define MVXPE_TX_CMD_L3_OFFSET_MASK	(0x7f << 0) /* offset of L3 hdr. */
#define MVXPE_TX_CMD_L3_OFFSET(v)	(((v) & 0x7f) << 0) 

/*
 * Transmit pakcet extra attributes
 * and error status returned from network controller.
 */
#define MVXPE_TX_F_DSA_TAG		(3 << 30)	/* DSA Tag */
/* bit 29:8 reserved */
#define MVXPE_TX_F_MH_SEL		(0xf << 4)	/* Marvell Header */
/* bit 3 reserved */
#define MVXPE_TX_F_EC_MASK		(3 << 1)	/* Error code */
#define MVXPE_TX_F_EC_LC		(0x00 << 1)	/* Late Collision */
#define MVXPE_TX_F_EC_UR		(0x01 << 1)	/* Underrun */
#define MVXPE_TX_F_EC_RL		(0x10 << 1)	/* Excess. Collision */
#define MVXPE_TX_F_EC_RESERVED		(0x11 << 1)
#define MVXPE_TX_F_ES			(1 << 0)	/* Error summary */

#define MVXPE_ERROR_SUMMARY		(1 << 0)
#define MVXPE_BUFFER_OWNED_MASK		(1 << 31)
#define MVXPE_BUFFER_OWNED_BY_HOST	(0 << 31)
#define MVXPE_BUFFER_OWNED_BY_DMA	(1 << 31)

#endif	/* _IF_MVXPEREG_H_ */
