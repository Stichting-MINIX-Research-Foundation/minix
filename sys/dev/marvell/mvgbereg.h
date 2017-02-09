/*	$NetBSD: mvgbereg.h,v 1.8 2013/12/23 02:23:25 kiyohara Exp $	*/
/*
 * Copyright (c) 2007, 2013 KIYOHARA Takashi
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
#ifndef _MVGBEREG_H_
#define _MVGBEREG_H_

#define MVGBE_SIZE		0x4000

#define MVGBE_NWINDOW		6
#define MVGBE_NREMAP		4

#define MVGBE_PHY_TIMEOUT	10000	/* msec */

/*
 * Ethernet Unit Registers
 */

#define MVGBE_PRXC(q)		(0x1400 + ((q) << 2)) /*Port RX queues Config*/
#define MVGBE_PRXSNP(q)		(0x1420 + ((q) << 2)) /* Port RX queues Snoop */
#define MVGBE_PRXF01(q)		(0x1440 + ((q) << 2)) /* Port RX Prefetch 0_1 */
#define MVGBE_PRXF23(q)		(0x1460 + ((q) << 2)) /* Port RX Prefetch 2_3 */
#define MVGBE_PRXDQA(q)		(0x1480 + ((q) << 2)) /*P RXqueues desc Q Addr*/
#define MVGBE_PRXDQS(q)		(0x14a0 + ((q) << 2)) /*P RXqueues desc Q Size*/
#define MVGBE_PRXDQTH(q)	(0x14c0 + ((q) << 2)) /*P RXqueues desc Q Thrs*/
#define MVGBE_PRXS(q)		(0x14e0 + ((q) << 2)) /*Port RX queues Status */
#define MVGBE_PRXSU(q)		(0x1500 + ((q) << 2)) /*P RXqueues Stat Update*/
#define MVGBE_PPLBSZ(q)		(0x1700 + ((q) << 2)) /* P Pool n Buffer Size */
#define MVGBE_PRXFC		0x1710	/* Port RX Flow Control */
#define MVGBE_PRXTXP		0x1714	/* Port RX_TX Pause */
#define MVGBE_PRXFCG		0x1718	/* Port RX Flow Control Generation */
#define MVGBE_PRXINIT		0x1cc0	/* Port RX Initialization */
#define MVGBE_RXCTRL		0x1d00	/* RX Control */
#define MVGBE_RXHWFWD(n)	(0x1d10 + (((n) & ~0x1) << 1))
				/* RX Hardware Forwarding (0_1, 2_3,..., 8_9) */
#define MVGBE_RXHWFWDPTR	0x1d30	/* RX Hardware Forwarding Pointer */
#define MVGBE_RXHWFWDTH		0x1d40	/* RX Hardware Forwarding Threshold */
#define MVGBE_RXHWFWDDQA	0x1d44	/* RX Hw Fwd Descriptors Queue Address*/
#define MVGBE_RXHWFWDQS		0x1d48	/* RX Hw Fwd Descriptors Queue Size */
#define MVGBE_RXHWFWDQENB	0x1d4c	/* RX Hw Fwd Queue Enable */
#define MVGBE_RXHWFWDACPT	0x1d50	/* RX Hw Forwarding Accepted Counter */
#define MVGBE_RXHWFWDYDSCRD	0x1d54	/* RX Hw Fwd Yellow Discarded Counter */
#define MVGBE_RXHWFWDGDSCRD	0x1d58	/* RX Hw Fwd Green Discarded Counter */
#define MVGBE_RXHWFWDTHDSCRD	0x1d5c	/*RX HwFwd Threshold Discarded Counter*/
#define MVGBE_RXHWFWDTXGAP	0x1d6c	/*RX Hardware Forwarding TX Access Gap*/

/* Ethernet Unit Global Registers */
#define MVGBE_PHYADDR		0x2000
#if defined(MV88W8660)
#define MVGBE_SMI		0x8010
#else
#define MVGBE_SMI		0x2004
#endif
#define MVGBE_EUDA		0x2008	/* Ethernet Unit Default Address */
#define MVGBE_EUDID		0x200c	/* Ethernet Unit Default ID */
#define MVGBE_EU 		0x2014	/* Ethernet Unit Reserved */
#define MVGBE_EUIC 		0x2080	/* Ethernet Unit Interrupt Cause */
#define MVGBE_EUIM 		0x2084	/* Ethernet Unit Interrupt Mask */
#define MVGBE_EUEA 		0x2094	/* Ethernet Unit Error Address */
#define MVGBE_EUIAE 		0x2098	/* Ethernet Unit Internal Addr Error */
#define MVGBE_EUPCR 		0x20a0	/* EthernetUnit Port Pads Calibration */
#define MVGBE_EUC 		0x20b0	/* Ethernet Unit Control */

#define MVGBE_BASEADDR(n)	(0x2200 + ((n) << 3))	/* Base Address */
#define MVGBE_S(n)		(0x2204 + ((n) << 3))	/* Size */
#define MVGBE_HA(n)		(0x2280 + ((n) << 2))	/* High Address Remap */
#define MVGBE_BARE 		0x2290	/* Base Address Enable */
#define MVGBE_EPAP 		0x2294	/* Ethernet Port Access Protect */

/* Ethernet Unit Port Registers */
#define MVGBE_PORTR_BASE	0x2400
#define MVGBE_PORTR_SIZE	 0x400

#define MVGBE_PXC		0x000	/* Port Configuration */
#define MVGBE_PXCX		0x004	/* Port Configuration Extend */
#define MVGBE_MIISP		0x008	/* MII Serial Parameters */
#define MVGBE_GMIISP		0x00c	/* GMII Serial Params */
#define MVGBE_EVLANE		0x010	/* VLAN EtherType */
#define MVGBE_MACAL		0x014	/* MAC Address Low */
#define MVGBE_MACAH		0x018	/* MAC Address High */
#define MVGBE_SDC		0x01c	/* SDMA Configuration */
#define MVGBE_DSCP(n)		(0x020 + ((n) << 2))
#define MVGBE_PSC		0x03c	/* Port Serial Control0 */
#define MVGBE_VPT2P		0x040	/* VLAN Priority Tag to Priority */
#define MVGBE_PS		0x044	/* Ethernet Port Status */
#define MVGBE_TQC		0x048	/* Transmit Queue Command */
#define MVGBE_PSC1		0x04c	/* Port Serial Control1 */
#define MVGBE_MH		0x054	/* Marvell Header */
#define MVGBE_MTU		0x058	/* Max Transmit Unit */
#define MVGBE_IC		0x060	/* Port Interrupt Cause */
#define MVGBE_ICE		0x064	/* Port Interrupt Cause Extend */
#define MVGBE_PIM		0x068	/* Port Interrupt Mask */
#define MVGBE_PEIM		0x06c	/* Port Extend Interrupt Mask */
#define MVGBE_PRFUT		0x070	/* Port Rx FIFO Urgent Threshold */
#define MVGBE_PTFUT		0x074	/* Port Tx FIFO Urgent Threshold */
#define MVGBE_PXTFTT		0x078	/* Port Tx FIFO Threshold */
#define MVGBE_PMFS		0x07c	/* Port Rx Minimal Frame Size */
#define MVGBE_PXDFC		0x084	/* Port Rx Discard Frame Counter */
#define MVGBE_POFC		0x088	/* Port Overrun Frame Counter */
#define MVGBE_PIAE		0x094	/* Port Internal Address Error */
#define MVGBE_AIP0ADR		0x098	/* Arp IP0 Address */
#define MVGBE_AIP1ADR		0x09c	/* Arp IP1 Address */
#define MVGBE_SERDESCFG		0x0a0	/* Serdes Configuration */
#define MVGBE_SERDESSTS		0x0a4	/* Serdes Status */
#define MVGBE_ETP		0x0bc	/* Ethernet Type Priority */
#define MVGBE_TQFPC		0x0dc	/* Transmit Queue Fixed Priority Cfg */
#define MVGBE_OMSCD		0x0f4	/* One mS Clock Divider */
#define MVGBE_PFCCD		0x0f8	/* Periodic Flow Control Clock Divider*/
#define MVGBE_PACC		0x100	/* Port Acceleration Mode */
#define MVGBE_PBMADDR		0x104	/* Port BM Address */
#define MVGBE_PV		0x1bc	/* Port Version */
#define MVGBE_CRDP(n)		(0x20c + ((n) << 4))
			/* Ethernet Current Receive Descriptor Pointers */
#define MVGBE_RQC		0x280	/* Receive Queue Command */
#define MVGBE_TCSDP		0x284	/* Tx Current Served Desc Pointer */
#define MVGBE_TCQDP		0x2c0	/* Tx Current Queue Desc Pointer */
#define MVGBE_TQTBCOUNT(q)	(0x300 + ((q) << 4))
				/* Transmit Queue Token-Bucket Counter */
#define MVGBE_TQTBCONFIG(q)	(0x304 + ((q) << 4))
				/* Transmit Queue Token-Bucket Configuration */
#define MVGBE_TQAC(q)		(0x308 + ((q) << 4))
				/* Transmit Queue Arbiter Configuration */

#define MVGBE_PCP2Q(cpu)	(0x2540 + ((cpu) << 2))	/* Port CPUn to Queue */
#define MVGBE_PRXITTH(q)	(0x2540 + ((q) << 2) /* Port RX Intr Threshold*/
#define MVGBE_PRXTXTIC		0x25a0	/*Port RX_TX Threshold Interrupt Cause*/
#define MVGBE_PRXTXTIM		0x25a4	/*Port RX_TX Threshold Interrupt Mask */
#define MVGBE_PRXTXIC		0x25a8	/* Port RX_TX Interrupt Cause */
#define MVGBE_PRXTXIM		0x25ac	/* Port RX_TX Interrupt Mask */
#define MVGBE_PMIC		0x25b0	/* Port Misc Interrupt Cause */
#define MVGBE_PMIM		0x25b4	/* Port Misc Interrupt Mask */
#define MVGBE_PIE		0x25b8	/* Port Interrupt Enable */

#define MVGBE_PMACC0		0x2c00	/* Port MAC Control 0 */
#define MVGBE_PMACC1		0x2c04	/* Port MAC Control 1 */
#define MVGBE_PMACC2		0x2c08	/* Port MAC Control 2 */
#define MVGBE_PANC		0x2c0c	/* Port Auto-Negotiation Configuration*/
#define MVGBE_PS0		0x2c10	/* Port Status 0 */
#define MVGBE_PSPC		0x2c14	/* Port Serial Parameters Config */
#define MVGBE_PIC_2		0x2c20	/* Port Interrupt Cause */
#define MVGBE_PIM_2		0x2c24	/* Port Interrupt Mask */
#define MVGBE_PPRBSS		0x2c38	/* Port PRBS Status */
#define MVGBE_PPRBSEC		0x2c3c	/* Port PRBS Error Counter */
#define MVGBE_PMACC3		0x2c48	/* Port MAC Control 3 */
#define MVGBE_CCFCPST(p)	(0x2c58 + ((p) << 2)) /*CCFC Port Speed Timerp*/
#define MVGBE_PMACC4		0x2c90	/* Port MAC Control 4 */
#define MVGBE_PSP1C		0x2c94	/* Port Serial Parameters 1 Config */
#define MVGBE_LPIC0		0x2cc0	/* LowPowerIdle control 0 */
#define MVGBE_LPIC1		0x2cc4	/* LPI control 1 */
#define MVGBE_LPIC2		0x2cc8	/* LPI control 2 */
#define MVGBE_LPIS		0x2ccc	/* LPI status */
#define MVGBE_LPIC		0x2cd0	/* LPI counter */

#define MVGBE_PPLLC		0x2e04	/* Power and PLL Control */
#define MVGBE_DLE		0x2e8c	/* Digital Loopback Enable */
#define MVGBE_RCS		0x2f18	/* Reference Clock Select */

/* MAC MIB Counters 		0x3000 - 0x307c */

/* Rx DMA Wake on LAN Registers	0x3690 - 0x36b8 */

#define MVGBE_PORTDAFR_BASE	0x3400
#define MVGBE_PORTDAFR_SIZE	 0x400

#define MVGBE_NDFSMT		 0x40
#define MVGBE_DFSMT		0x000
			/* Destination Address Filter Special Multicast Table */
#define MVGBE_NDFOMT		 0x40
#define MVGBE_DFOMT		0x100
			/* Destination Address Filter Other Multicast Table */
#define MVGBE_NDFUT		  0x4
#define MVGBE_DFUT		0x200
			/* Destination Address Filter Unicast Table */

#define MVGBE_PTXDQA(q)		(0x3c00 + ((q) << 2)) /*P TXqueues desc Q Addr*/
#define MVGBE_PTXDQS(q)		(0x3c20 + ((q) << 2)) /*P TXqueues desc Q Size*/
#define MVGBE_PTXS(q)		(0x3c40 + ((q) << 2)) /* Port TX queues Status*/
#define MVGBE_PTXSU(q)		(0x3c60 + ((q) << 2)) /*P TXqueues Stat Update*/
#define MVGBE_PTXDI(q)		(0x3c80 + ((q) << 2)) /* P TXqueues Desc Index*/
#define MVGBE_TXTBC(q)		(0x3ca0 + ((q) << 2)) /* TX Trans-ed Buf Count*/
#define MVGBE_PTXINIT		0x3cf0	/* Port TX Initialization */
#define MVGBE_PTXDOSD		0x3cf4	/* Port TX Disable Outstanding Reads */

#define MVGBE_TXBADFCS		0x3cc0	/*Tx Bad FCS Transmitted Pckts Counter*/
#define MVGBE_TXDROPPED		0x3cc4	/* Tx Dropped Packets Counter */
#define MVGBE_TXNB		0x3cfc	/* Tx Number of New Bytes */
#define MVGBE_TXGB		0x3d00	/* Tx Green Number of Bytes */
#define MVGBE_TXYB		0x3d04	/* Tx Yellow Number of Bytes */

/* Tx DMA Packet Modification Registers	0x3d00 - 0x3dff */

/* Tx DMA Queue Arbiter Registers	0x3e00 - 0x3eff */


/* PHY Address (MVGBE_PHYADDR) */
#define MVGBE_PHYADDR_PHYAD_MASK	0x1f
#define MVGBE_PHYADDR_PHYAD(port, phy)	((phy) << ((port) * 5))

/* SMI register fields (MVGBE_SMI) */
#define MVGBE_SMI_DATA_MASK		0x0000ffff
#define MVGBE_SMI_PHYAD(phy)		(((phy) & 0x1f) << 16)
#define MVGBE_SMI_REGAD(reg)		(((reg) & 0x1f) << 21)
#define MVGBE_SMI_OPCODE_WRITE		(0 << 26)
#define MVGBE_SMI_OPCODE_READ		(1 << 26)
#define MVGBE_SMI_READVALID		(1 << 27)
#define MVGBE_SMI_BUSY			(1 << 28)

/* Ethernet Unit Default ID (MVGBE_EUDID) */
#define MVGBE_EUDID_DIDR_MASK		0x0000000f
#define MVGBE_EUDID_DATTR_MASK		0x00000ff0

/* Ethernet Unit Reserved (MVGBE_EU) */
#define MVGBE_EU_FASTMDC 		(1 << 0)
#define MVGBE_EU_ACCS 			(1 << 1)

/* Ethernet Unit Interrupt Cause (MVGBE_EUIC) */
#define MVGBE_EUIC_ETHERINTSUM 		(1 << 0)
#define MVGBE_EUIC_PARITY 		(1 << 1)
#define MVGBE_EUIC_ADDRVIOL		(1 << 2)
#define MVGBE_EUIC_ADDRVNOMATCH		(1 << 3)
#define MVGBE_EUIC_SMIDONE		(1 << 4)
#define MVGBE_EUIC_COUNTWA		(1 << 5)
#define MVGBE_EUIC_INTADDRERR		(1 << 7)
#define MVGBE_EUIC_PORT0DPERR		(1 << 9)
#define MVGBE_EUIC_TOPDPERR		(1 << 12)

/* Ethernet Unit Internal Addr Error (MVGBE_EUIAE) */
#define MVGBE_EUIAE_INTADDR_MASK 	0x000001ff

/* Ethernet Unit Port Pads Calibration (MVGBE_EUPCR) */
#define MVGBE_EUPCR_DRVN_MASK		0x0000001f
#define MVGBE_EUPCR_TUNEEN		(1 << 16)
#define MVGBE_EUPCR_LOCKN_MASK		0x003e0000
#define MVGBE_EUPCR_OFFSET_MASK		0x1f000000	/* Reserved */
#define MVGBE_EUPCR_WREN		(1 << 31)

/* Ethernet Unit Control (MVGBE_EUC) */
#define MVGBE_EUC_PORT0DPPAR 		(1 << 0)
#define MVGBE_EUC_POLLING	 	(1 << 1)
#define MVGBE_EUC_TOPDPPAR	 	(1 << 3)
#define MVGBE_EUC_PORT0PW 		(1 << 16)
#define MVGBE_EUC_PORTRESET	 	(1 << 24)
#define MVGBE_EUC_RAMSINITIALIZATIONCOMPLETED (1 << 25)

/* Base Address (MVGBE_BASEADDR) */
#define MVGBE_BASEADDR_TARGET(target)	((target) & 0xf)
#define MVGBE_BASEADDR_ATTR(attr)	(((attr) & 0xff) << 8)
#define MVGBE_BASEADDR_BASE(base)	((base) & 0xffff0000)

/* Size (MVGBE_S) */
#define MVGBE_S_SIZE(size)		(((size) - 1) & 0xffff0000)

/* Base Address Enable (MVGBE_BARE) */
#define MVGBE_BARE_EN_MASK		((1 << MVGBE_NWINDOW) - 1)
#define MVGBE_BARE_EN(win)		((1 << (win)) & MVGBE_BARE_EN_MASK)

/* Ethernet Port Access Protect (MVGBE_EPAP) */
#define MVGBE_EPAP_AC_NAC		0x0	/* No access allowed */
#define MVGBE_EPAP_AC_RO		0x1	/* Read Only */
#define MVGBE_EPAP_AC_FA		0x3	/* Full access (r/w) */
#define MVGBE_EPAP_EPAR(win, ac)	((ac) << ((win) * 2))

/* Port Configuration (MVGBE_PXC) */
#define MVGBE_PXC_UPM			(1 << 0) /* Uni Promisc mode */
#define MVGBE_PXC_RXQ(q)		((q) << 1)
#define MVGBE_PXC_RXQ_MASK		MVGBE_PXC_RXQ(7)
#define MVGBE_PXC_RXQARP(q)		((q) << 4)
#define MVGBE_PXC_RXQARP_MASK		MVGBE_PXC_RXQARP(7)
#define MVGBE_PXC_RB			(1 << 7) /* Rej mode of MAC */
#define MVGBE_PXC_RBIP			(1 << 8)
#define MVGBE_PXC_RBARP			(1 << 9)
#define MVGBE_PXC_AMNOTXES		(1 << 12)
#define MVGBE_PXC_RBARPF		(1 << 13)
#define MVGBE_PXC_TCPCAPEN		(1 << 14)
#define MVGBE_PXC_UDPCAPEN		(1 << 15)
#define MVGBE_PXC_TCPQ(q)		((q) << 16)
#define MVGBE_PXC_TCPQ_MASK		MVGBE_PXC_TCPQ(7)
#define MVGBE_PXC_UDPQ(q)		((q) << 19)
#define MVGBE_PXC_UDPQ_MASK		MVGBE_PXC_UDPQ(7)
#define MVGBE_PXC_BPDUQ(q)		((q) << 22)
#define MVGBE_PXC_BPDUQ_MASK		MVGBE_PXC_BPDUQ(7)
#define MVGBE_PXC_RXCS			(1 << 25)

/* Port Configuration Extend (MVGBE_PXCX) */
#define MVGBE_PXCX_SPAN			(1 << 1)
#define MVGBE_PXCX_TXCRCDIS		(1 << 3)

/* MII Serial Parameters (MVGBE_MIISP) */
#define MVGBE_MIISP_JAMLENGTH_12KBIT	0x00000000
#define MVGBE_MIISP_JAMLENGTH_24KBIT	0x00000001
#define MVGBE_MIISP_JAMLENGTH_32KBIT	0x00000002
#define MVGBE_MIISP_JAMLENGTH_48KBIT	0x00000003
#define MVGBE_MIISP_JAMIPG(x)		(((x) & 0x7c) << 0)
#define MVGBE_MIISP_IPGJAMTODATA(x)	(((x) & 0x7c) << 5)
#define MVGBE_MIISP_IPGDATA(x)		(((x) & 0x7c) << 10)
#define MVGBE_MIISP_DATABLIND(x)	(((x) & 0x1f) << 17)

/* GMII Serial Parameters (MVGBE_GMIISP) */
#define MVGBE_GMIISP_IPGDATA(x)		(((x) >> 4) & 0x7)

/* SDMA Configuration (MVGBE_SDC) */
#define MVGBE_SDC_RIFB			(1 << 0)
#define MVGBE_SDC_RXBSZ(x)		((x) << 1)
#define MVGBE_SDC_RXBSZ_MASK		MVGBE_SDC_RXBSZ(7)
#define MVGBE_SDC_RXBSZ_1_64BITWORDS	MVGBE_SDC_RXBSZ(0)
#define MVGBE_SDC_RXBSZ_2_64BITWORDS	MVGBE_SDC_RXBSZ(1)
#define MVGBE_SDC_RXBSZ_4_64BITWORDS	MVGBE_SDC_RXBSZ(2)
#define MVGBE_SDC_RXBSZ_8_64BITWORDS	MVGBE_SDC_RXBSZ(3)
#define MVGBE_SDC_RXBSZ_16_64BITWORDS	MVGBE_SDC_RXBSZ(4)
#define MVGBE_SDC_BLMR			(1 << 4)
#define MVGBE_SDC_BLMT			(1 << 5)
#define MVGBE_SDC_SWAPMODE		(1 << 6)
#define MVGBE_SDC_IPGINTRX_V1_MASK	__BITS(21, 8)
#define MVGBE_SDC_IPGINTRX_V2_MASK	(__BIT(25) | __BITS(21, 7))
#define MVGBE_SDC_IPGINTRX_V1(x)	(((x) << 4)			\
						& MVGBE_SDC_IPGINTRX_V1_MASK)
#define MVGBE_SDC_IPGINTRX_V2(x)	((((x) & 0x8000) << 10) 	\
						| (((x) & 0x7fff) << 7))
#define MVGBE_SDC_IPGINTRX_V1_MAX	0x3fff
#define MVGBE_SDC_IPGINTRX_V2_MAX	0xffff
#define MVGBE_SDC_TXBSZ(x)		((x) << 22)
#define MVGBE_SDC_TXBSZ_MASK		MVGBE_SDC_TXBSZ(7)
#define MVGBE_SDC_TXBSZ_1_64BITWORDS	MVGBE_SDC_TXBSZ(0)
#define MVGBE_SDC_TXBSZ_2_64BITWORDS	MVGBE_SDC_TXBSZ(1)
#define MVGBE_SDC_TXBSZ_4_64BITWORDS	MVGBE_SDC_TXBSZ(2)
#define MVGBE_SDC_TXBSZ_8_64BITWORDS	MVGBE_SDC_TXBSZ(3)
#define MVGBE_SDC_TXBSZ_16_64BITWORDS	MVGBE_SDC_TXBSZ(4)

/* Port Serial Control (MVGBE_PSC) */
#define MVGBE_PSC_PORTEN		(1 << 0)
#define MVGBE_PSC_FLP			(1 << 1) /* Force_Link_Pass */
#define MVGBE_PSC_ANDUPLEX		(1 << 2)	/* auto nego */
#define MVGBE_PSC_ANFC			(1 << 3)
#define MVGBE_PSC_PAUSEADV		(1 << 4)
#define MVGBE_PSC_FFCMODE		(1 << 5)	/* Force FC */
#define MVGBE_PSC_FBPMODE		(1 << 7)	/* Back pressure */
#define MVGBE_PSC_RESERVED		(1 << 9)	/* Must be set to 1 */
#define MVGBE_PSC_FLFAIL		(1 << 10)	/* Force Link Fail */
#define MVGBE_PSC_ANSPEED		(1 << 13)
#define MVGBE_PSC_DTEADVERT		(1 << 14)
#define MVGBE_PSC_MRU(x)		((x) << 17)
#define MVGBE_PSC_MRU_MASK		MVGBE_PSC_MRU(7)
#define MVGBE_PSC_MRU_1518		0
#define MVGBE_PSC_MRU_1522		1
#define MVGBE_PSC_MRU_1552		2
#define MVGBE_PSC_MRU_9022		3
#define MVGBE_PSC_MRU_9192		4
#define MVGBE_PSC_MRU_9700		5
#define MVGBE_PSC_SETFULLDX		(1 << 21)
#define MVGBE_PSC_SETFCEN		(1 << 22)
#define MVGBE_PSC_SETGMIISPEED		(1 << 23)
#define MVGBE_PSC_SETMIISPEED		(1 << 24)

/* Ethernet Port Status (MVGBE_PS) */
#define MVGBE_PS_LINKUP			(1 << 1)
#define MVGBE_PS_FULLDX			(1 << 2)
#define MVGBE_PS_ENFC			(1 << 3)
#define MVGBE_PS_GMIISPEED		(1 << 4)
#define MVGBE_PS_MIISPEED		(1 << 5)
#define MVGBE_PS_TXINPROG		(1 << 7)
#define MVGBE_PS_TXFIFOEMP		(1 << 10)	/* FIFO Empty */
#define MVGBE_PS_RXFIFOEMPTY		(1 << 16)
/* Armada XP */
#define MVGBE_PS_TXINPROG_MASK		(0xff << 0)
#define MVGBE_PS_TXINPROG_(q)		(1 << ((q) + 0))
#define MVGBE_PS_TXFIFOEMP_MASK		(0xff << 8)
#define MVGBE_PS_TXFIFOEMP_(q)		(1 << ((q) + 8))

/* Transmit Queue Command (MVGBE_TQC) */
#define MVGBE_TQC_ENQ(q)		(1 << ((q) + 0))/* Enable Q */
#define MVGBE_TQC_DISQ(q)		(1 << ((q) + 8))/* Disable Q */

/* Port Serial Control 1 (MVGBE_PSC1) */
#define MVGBE_PSC1_PCSLB		(1 << 1)
#define MVGBE_PSC1_RGMIIEN		(1 << 3)	/* RGMII */
#define MVGBE_PSC1_PRST			(1 << 4)	/* Port Reset */

/* Port Interrupt Cause (MVGBE_IC) */
#define MVGBE_IC_RXBUF			(1 << 0)
#define MVGBE_IC_EXTEND			(1 << 1)
#define MVGBE_IC_RXBUFQ_MASK		(0xff << 2)
#define MVGBE_IC_RXBUFQ(q)		(1 << ((q) + 2))
#define MVGBE_IC_RXERROR		(1 << 10)
#define MVGBE_IC_RXERRQ_MASK		(0xff << 11)
#define MVGBE_IC_RXERRQ(q)		(1 << ((q) + 11))
#define MVGBE_IC_TXEND(q)		(1 << ((q) + 19))
#define MVGBE_IC_ETHERINTSUM		(1 << 31)

/* Port Interrupt Cause Extend (MVGBE_ICE) */
#define MVGBE_ICE_TXBUF_MASK		(0xff << + 0)
#define MVGBE_ICE_TXBUF(q)		(1 << ((q) + 0))
#define MVGBE_ICE_TXERR_MASK		(0xff << + 8)
#define MVGBE_ICE_TXERR(q)		(1 << ((q) + 8))
#define MVGBE_ICE_PHYSTC		(1 << 16)
#define MVGBE_ICE_PTP			(1 << 17)
#define MVGBE_ICE_RXOVR			(1 << 18)
#define MVGBE_ICE_TXUDR			(1 << 19)
#define MVGBE_ICE_LINKCHG		(1 << 20)
#define MVGBE_ICE_SERDESREALIGN		(1 << 21)
#define MVGBE_ICE_INTADDRERR		(1 << 23)
#define MVGBE_ICE_SYNCCHANGED		(1 << 24)
#define MVGBE_ICE_PRBSERROR		(1 << 25)
#define MVGBE_ICE_ETHERINTSUM		(1 << 31)

/* Port Tx FIFO Urgent Threshold (MVGBE_PTFUT) */
#define MVGBE_PTFUT_IPGINTTX_V1_MASK	__BITS(17, 4)
#define MVGBE_PTFUT_IPGINTTX_V2_MASK	__BITS(19, 4)
#define MVGBE_PTFUT_IPGINTTX_V1(x)   __SHIFTIN(x, MVGBE_PTFUT_IPGINTTX_V1_MASK)
#define MVGBE_PTFUT_IPGINTTX_V2(x)   __SHIFTIN(x, MVGBE_PTFUT_IPGINTTX_V2_MASK)
#define MVGBE_PTFUT_IPGINTTX_V1_MAX	0x3fff
#define MVGBE_PTFUT_IPGINTTX_V2_MAX	0xffff

/* Port Rx Minimal Frame Size (MVGBE_PMFS) */
#define MVGBE_PMFS_RXMFS(rxmfs)		(((rxmfs) - 40) & 0x7c)
					/* RxMFS = 40,44,48,52,56,60,64 bytes */

/* Transmit Queue Fixed Priority Configuration */
#define MVGBE_TQFPC_EN(q)		(1 << (q))

/* Receive Queue Command (MVGBE_RQC) */
#define MVGBE_RQC_ENQ_MASK		(0xff << 0)	/* Enable Q */
#define MVGBE_RQC_ENQ(n)		(1 << (0 + (n)))
#define MVGBE_RQC_DISQ_MASK		(0xff << 8)	/* Disable Q */
#define MVGBE_RQC_DISQ(n)		(1 << (8 + (n)))
#define MVGBE_RQC_DISQ_DISABLE(q)	((q) << 8)

/* Destination Address Filter Registers (MVGBE_DF{SM,OM,U}T) */
#define MVGBE_DF(n, x)			((x) << (8 * (n)))
#define MVGBE_DF_PASS			(1 << 0)
#define MVGBE_DF_QUEUE(q)		((q) << 1)
#define MVGBE_DF_QUEUE_MASK		((7) << 1)


/* Port Acceleration Mode (MVGBE_PACC) */
#define MVGVE_PACC_ACCELERATIONMODE_MASK	0x7
#define MVGVE_PACC_ACCELERATIONMODE_BM		0x0	/* Basic Mode */
#define MVGVE_PACC_ACCELERATIONMODE_EDM		0x1	/* Enhanced Desc Mode */
#define MVGVE_PACC_ACCELERATIONMODE_EDMBM	0x2	/*   with BM */
#define MVGVE_PACC_ACCELERATIONMODE_EDMPNC	0x3	/*   with PnC */
#define MVGVE_PACC_ACCELERATIONMODE_EDMBPMNC	0x4	/*   with BM & PnC */

/* Port BM Address (MVGBE_PBMADDR) */
#define MVGBE_PBMADDR_BMADDRESS_MASK	0xfffff800

/* Ether Type Priority (MVGBE_ETP) */
#define MVGBE_ETP_ETHERTYPEPRIEN	(1 << 0)	/* EtherType Prio Ena */
#define MVGBE_ETP_ETHERTYPEPRIFRSTEN	(1 << 1)
#define MVGBE_ETP_ETHERTYPEPRIQ		(0x7 << 2)	/*EtherType Prio Queue*/
#define MVGBE_ETP_ETHERTYPEPRIVAL	(0xffff << 5)	/*EtherType Prio Value*/
#define MVGBE_ETP_FORCEUNICSTHIT	(1 << 21)	/* Force Unicast hit */

/* RX Hardware Forwarding (0_1, 2_3,..., 8_9) (MVGBE_RXHWFWD) */
#define MVGBE_RXHWFWD_PORT_BASEADDRESS(p, x)	xxxxx

/* RX Hardware Forwarding Pointer (MVGBE_RXHWFWDPTR) */
#define MVGBE_RXHWFWDPTR_QUEUENO(q)	((q) << 8)	/* Queue Number */
#define MVGBE_RXHWFWDPTR_PORTNO(p)	((p) << 11)	/* Port Number */

/* RX Hardware Forwarding Threshold (MVGBE_RXHWFWDTH) */
#define MVGBE_RXHWFWDTH_DROPRNDGENBITS(n)	(((n) & 0x3ff) << 0)
#define MVGBE_RXHWFWDTH_DROPTHRESHOLD(n)	(((n) & 0xf) << 16)

/* RX Control (MVGBE_RXCTRL) */
#define MVGBE_RXCTRL_PACKETCOLORSRCSELECT(x) (1 << 0)
#define MVGBE_RXCTRL_GEMPORTIDSRCSEL(x)	((x) << 4)
#define MVGBE_RXCTRL_TXHWFRWMQSRC(x)	(1 << 8)
#define MVGBE_RXCTRL_RX_MH_SELECT(x)	((x) << 12)
#define MVGBE_RXCTRL_RX_TX_SRC_SELECT	(1 << 16)
#define MVGBE_RXCTRL_HWFRWDENB		(1 << 17)
#define MVGBE_RXCTRL_HWFRWDSHORTPOOLID(id) (((id) & 0x3) << 20)
#define MVGBE_RXCTRL_HWFRWDLONGPOOLID(id) (((id) & 0x3) << 22)

/* Port RX queues Configuration (MVGBE_PRXC) */
#define MVGBE_PRXC_POOLIDSHORT(i)	(((i) & 0x3) << 4)
#define MVGBE_PRXC_POOLIDLONG(i)	(((i) & 0x3) << 6)
#define MVGBE_PRXC_PACKETOFFSET(o)	(((o) & 0xf) << 8)
#define MVGBE_PRXC_USERPREFETCHCMND0	(1 << 16)

/* Port RX queues Snoop (MVGBE_PRXSNP) */
#define MVGBE_PRXSNP_SNOOPNOOFBYTES(b)	(((b) & 0x3fff) << 0)
#define MVGBE_PRXSNP_L2DEPOSITNOOFBYTES(b) (((b) & 0x3fff) << 16)

/* Port RX queues Snoop (MVGBE_PRXSNP) */
#define MVGBE_PRXF01_PREFETCHCOMMAND0(c) (((c) & 0xffff) << 0) xxxx
#define MVGBE_PRXF01_PREFETCHCOMMAND1(c) (((c) & 0xffff) << 16) xxxx

/* Port RX queues Descriptors Queue Size (MVGBE_PRXDQS) */
#define MVGBE_PRXDQS_DESCRIPTORSQUEUESIZE(s) (((s) & 0x0003fff) << 0)
#define MVGBE_PRXDQS_BUFFERSIZE(s)	(((s) & 0xfff80000) << 19)

/* Port RX queues Descriptors Queue Threshold (MVGBE_PRXDQTH) */
					/* Occupied Descriptors Threshold */
#define MVGBE_PRXDQTH_ODT(x)		(((x) & 0x3fff) << 0)
					/* Non Occupied Descriptors Threshold */
#define MVGBE_PRXDQTH_NODT(x)		(((x) & 0x3fff) << 16)

/* Port RX queues Status (MVGBE_PRXS) */
					/* Occupied Descriptors Counter */
#define MVGBE_PRXS_ODC(x)		(((x) & 0x3fff) << 0)
					/* Non Occupied Descriptors Counter */
#define MVGBE_PRXS_NODC(x)		(((x) & 0x3fff) << 16)

/* Port RX queues Status Update (MVGBE_PRXSU) */
#define MVGBE_PRXSU_NOOFPROCESSEDDESCRIPTORS(x) (((x) & 0xff) << 0)
#define MVGBE_PRXSU_NOOFNEWDESCRIPTORS(x) (((x) & 0xff) << 16)

/* Port RX Flow Control (MVGBE_PRXFC) */
#define MVGBE_PRXFC_PERPRIOFCGENCONTROL	(1 << 0)
#define MVGBE_PRXFC_TXPAUSECONTROL	(1 << 1)

/* Port RX_TX Pause (MVGBE_PRXTXP) */
#define MVGBE_PRXTXP_TXPAUSE(x)		((x) & 0xff)

/* Port RX Flow Control Generation (MVGBE_PRXFCG) */
#define MVGBE_PRXFCG_PERPRIOFCGENDATA	(1 << 0)
#define MVGBE_PRXFCG_PERPRIOFCGENQNO(x)	(((x) & 0x7) << 4)

/* Port RX Initialization (MVGBE_PRXINIT) */
#define MVGBE_PRXINIT_RXDMAINIT		(1 << 0)

/* TX Number of New Bytes (MVGBE_TXNB) */
#define MVGBE_TXNB_NOOFNEWBYTES(b)	(((b) & 0xffff) << 0)
#define MVGBE_TXNB_PKTQNO(q)		(((q) & 0x7) << 28)
#define MVGBE_TXNB_PKTCOLOR		(1 << 31)

/* Port TX queues Descriptors Queue Size (MVGBE_PTXDQS) */
					/* Descriptors Queue Size */
#define MVGBE_PTXDQS_DQS(x)		(((x) & 0x3fff) << 0)
					/* Transmitted Buffer Threshold */
#define MVGBE_PTXDQS_TBT(x)		(((x) & 0x3fff) << 16)

/* Port TX queues Status (MVGBE_PTXS) */
					/* Pending Descriptors Counter */
#define MVGBE_PTXDQS_PDC(x)		(((x) & 0x3fff) << 0)
					/* Transmitted Buffer Counter */
#define MVGBE_PTXS_TBC(x)		(((x) & 0x3fff) << 16)

/* Port TX queues Status Update (MVGBE_PTXSU) */
					/* Number Of Written Descriptoes */
#define MVGBE_PTXSU_NOWD(x)		(((x) & 0xff) << 0)
					/* Number Of Released Buffers */
#define MVGBE_PTXSU_NORB(x)		(((x) & 0xff) << 16)

/* TX Transmitted Buffers Counter (MVGBE_TXTBC) */
					/* Transmitted Buffers Counter */
#define MVGBE_TXTBC_TBC(x)		(((x) & 0x3fff) << 16)

/* Port TX Initialization (MVGBE_PTXINIT) */
#define MVGBE_PTXINIT_TXDMAINIT		(1 << 0)

/* Marvell Header (MVGBE_MH) */
#define MVGBE_MH_MHEN			(1 << 0)
#define MVGBE_MH_DAPREFIX		(0x3 << 1)
#define MVGBE_MH_SPID			(0xf << 4)
#define MVGBE_MH_MHMASK			(0x3 << 8)
#define MVGBE_MH_MHMASK_8QUEUES		(0x0 << 8)
#define MVGBE_MH_MHMASK_4QUEUES		(0x1 << 8)
#define MVGBE_MH_MHMASK_2QUEUES		(0x3 << 8)
#define MVGBE_MH_DSAEN_MASK		(0x3 << 10)
#define MVGBE_MH_DSAEN_DISABLE		(0x0 << 10)
#define MVGBE_MH_DSAEN_NONEXTENDED	(0x1 << 10)
#define MVGBE_MH_DSAEN_EXTENDED		(0x2 << 10)

/* Port Auto-Negotiation Configuration (MVGBE_PANC) */
#define MVGBE_PANC_FORCELINKFAIL	(1 << 0)
#define MVGBE_PANC_FORCELINKPASS	(1 << 1)
#define MVGBE_PANC_INBANDANEN		(1 << 2)
#define MVGBE_PANC_INBANDANBYPASSEN	(1 << 3)
#define MVGBE_PANC_INBANDRESTARTAN	(1 << 4)
#define MVGBE_PANC_SETMIISPEED		(1 << 5)
#define MVGBE_PANC_SETGMIISPEED		(1 << 6)
#define MVGBE_PANC_ANSPEEDEN		(1 << 7)
#define MVGBE_PANC_SETFCEN		(1 << 8)
#define MVGBE_PANC_PAUSEADV		(1 << 9)
#define MVGBE_PANC_ANFCEN		(1 << 11)
#define MVGBE_PANC_SETFULLDX		(1 << 12)
#define MVGBE_PANC_ANDUPLEXEN		(1 << 13)
#define MVGBE_PANC_RESERVED		(1 << 15)

/* Port MAC Control 0 (MVGBE_PMACC0) */
#define MVGBE_PMACC0_PORTEN		(1 << 0)
#define MVGBE_PMACC0_PORTTYPE		(1 << 1)
#define MVGBE_PMACC0_FRAMESIZELIMIT(x)	((((x) >> 1) & 0x7ffc) << 2)
#define MVGBE_PMACC0_RESERVED		(1 << 15)

/* Port MAC Control 1 (MVGBE_PMACC1) */
#define MVGBE_PMACC1_PCSLB		(1 << 6)

/* Port MAC Control 2 (MVGBE_PMACC2) */
#define MVGBE_PMACC2_PCSEN		(1 << 3)
#define MVGBE_PMACC2_RGMIIEN		(1 << 4)
#define MVGBE_PMACC2_PADDINGDIS		(1 << 5)
#define MVGBE_PMACC2_PORTMACRESET	(1 << 6)
#define MVGBE_PMACC2_PRBSCHECKEN	(1 << 10)
#define MVGBE_PMACC2_PRBSGENEN		(1 << 11)
#define MVGBE_PMACC2_SDTT_MASK		(3 << 12)  /* Select Data To Transmit */
#define MVGBE_PMACC2_SDTT_RM		(0 << 12)	/* Regular Mode */
#define MVGBE_PMACC2_SDTT_PRBS		(1 << 12)	/* PRBS Mode */
#define MVGBE_PMACC2_SDTT_ZC		(2 << 12)	/* Zero Constant */
#define MVGBE_PMACC2_SDTT_OC		(3 << 12)	/* One Constant */
#define MVGBE_PMACC2_RESERVED		(3 << 14)

/* Port MAC Control 3 (MVGBE_PMACC3) */
#define MVGBE_PMACC3_IPG_MASK		0x7f80

/* Port Interrupt Cause/Mask (MVGBE_PIC_2/MVGBE_PIM_2) */
#define MVGBE_PI_2_INTSUM		(1 << 0)
#define MVGBE_PI_2_LSC			(1 << 1)   /* LinkStatus Change */
#define MVGBE_PI_2_ACOP			(1 << 2)   /* AnCompleted OnPort */
#define MVGBE_PI_2_AOOR			(1 << 5)   /* AddressOut Of Range */
#define MVGBE_PI_2_SSC			(1 << 6)   /* SyncStatus Change */
#define MVGBE_PI_2_PRBSEOP		(1 << 7)   /* QSGMII PRBS error */
#define MVGBE_PI_2_MIBCWA		(1 << 15)  /* MIB counter wrap around */
#define MVGBE_PI_2_QSGMIIPRBSE		(1 << 10)  /* QSGMII PRBS error */
#define MVGBE_PI_2_PCSRXPRLPI		(1 << 11)  /* PCS Rx path received LPI*/
#define MVGBE_PI_2_PCSTXPRLPI		(1 << 12)  /* PCS Tx path received LPI*/
#define MVGBE_PI_2_MACRXPRLPI		(1 << 13)  /* MAC Rx path received LPI*/
#define MVGBE_PI_2_MIBCCD		(1 << 14)  /* MIB counters copy done */

/* LPI Control 0 (MVGBE_LPIC0) */
#define MVGBE_LPIC0_LILIMIT(x)		(((x) & 0xff) << 0)
#define MVGBE_LPIC0_TSLIMIT(x)		(((x) & 0xff) << 8)

/* LPI Control 1 (MVGBE_LPIC1) */
#define MVGBE_LPIC1_LPIRE		(1 << 0)	/* LPI request enable */
#define MVGBE_LPIC1_LPIRF		(1 << 1)	/* LPI request force */
#define MVGBE_LPIC1_LPIMM		(1 << 2)	/* LPI manual mode */
#define MVGBE_LPIC1_TWLIMIT		(((x) & 0xfff) << 4)

/* LPI Status (MVGBE_LPIS) */
#define MVGBE_LPIS_PCSRXPLPIS		(1 << 0) /* PCS Rx path LPI status */
#define MVGBE_LPIS_PCSTXPLPIS		(1 << 1) /* PCS Tx path LPI status */
#define MVGBE_LPIS_MACRXPLPIS		(1 << 2)/* MAC Rx path LP idle status */
#define MVGBE_LPIS_MACTXPLPWS		(1 << 3)/* MAC Tx path LP wait status */
#define MVGBE_LPIS_MACTXPLPIS		(1 << 4)/* MAC Tx path LP idle status */

/* Port PRBS Status (MVGBE_PPRBSS) */
#define MVGBE_PPRBSS_PRBSCHECKLOCKED	(1 << 0)
#define MVGBE_PPRBSS_PRBSCHECKRDY	(1 << 1)

/* Port Status 0 (MVGBE_PS0) */
#define MVGBE_PS0_LINKUP		(1 << 0)
#define MVGBE_PS0_GMIISPEED		(1 << 1)
#define MVGBE_PS0_MIISPEED		(1 << 2)
#define MVGBE_PS0_FULLDX		(1 << 3)
#define MVGBE_PS0_RXFCEN		(1 << 4)
#define MVGBE_PS0_TXFCEN		(1 << 5)
#define MVGBE_PS0_PRP			(1 << 6) /* Port Rx Pause */
#define MVGBE_PS0_PTP			(1 << 7) /* Port Tx Pause */
#define MVGBE_PS0_PDP			(1 << 8) /*Port is Doing Back-Pressure*/
#define MVGBE_PS0_SYNCFAIL10MS		(1 << 10)
#define MVGBE_PS0_ANDONE		(1 << 11)
#define MVGBE_PS0_IBANBA		(1 << 12) /* InBand AutoNeg BypassAct */
#define MVGBE_PS0_SYNCOK		(1 << 14)

/* Port CPUn to Queue (MVGBE_PCP2Q) */
#define MVGBE_PCP2Q_RXQAE(q)		(1 << ((q) + << 0))/*QueueAccessEnable*/
#define MVGBE_PCP2Q_TXQAE(q)		(1 << ((q) + << 8))/*QueueAccessEnable*/

/* Port RX_TX Threshold Interrupt Cause/Mask (MVGBE_PRXTXTIC/MVGBE_PRXTXTIM) */
#define MVGBE_PRXTXTI_TBTCQ(q)		(1 << ((q) + 0))
#define MVGBE_PRXTXTI_RBICTAPQ(q)	(1 << ((q) + 8))
#define MVGBE_PRXTXTI_RDTAQ(q)		(1 << ((q) + 16))
#define MVGBE_PRXTXTI_PRXTXICSUMMARY	(1 << 29)
#define MVGBE_PRXTXTI_PTXERRORSUMMARY	(1 << 30)
#define MVGBE_PRXTXTI_PMISCICSUMMARY	(1 << 31)

/* Port RX_TX Interrupt Cause/Mask (MVGBE_PRXTXIC/MVGBE_PRXTXIM) */
#define MVGBE_PRXTXI_TBRQ(q)		(1 << ((q) + 0))
#define MVGBE_PRXTXI_RPQ(q)		(1 << ((q) + 8))
#define MVGBE_PRXTXI_RREQ(q)		(1 << ((q) + 16))
#define MVGBE_PRXTXI_PRXTXTHICSUMMARY	(1 << 29)
#define MVGBE_PRXTXI_PTXERRORSUMMARY	(1 << 30)
#define MVGBE_PRXTXI_PMISCICSUMMARY	(1 << 31)

/* Port Misc Interrupt Cause/Mask (MVGBE_PMIC/MVGBE_PMIM) */
#define MVGBE_PMI_PHYSTATUSCHNG		(1 << 0)
#define MVGBE_PMI_LINKCHANGE		(1 << 1)
#define MVGBE_PMI_PTP			(1 << 4)
#define MVGBE_PMI_PME			(1 << 6) /* Packet Modification Error */
#define MVGBE_PMI_IAE			(1 << 7) /* Internal Address Error */
#define MVGBE_PMI_RXOVERRUN		(1 << 8)
#define MVGBE_PMI_RXCRCERROR		(1 << 9)
#define MVGBE_PMI_RXLARGEPACKET		(1 << 10)
#define MVGBE_PMI_TXUNDRN		(1 << 11)
#define MVGBE_PMI_PRBSERROR		(1 << 12)
#define MVGBE_PMI_SRSE			(1 << 14) /* SerdesRealignSyncError */
#define MVGBE_PMI_RNBTP(q)		(1 << ((q) + 16)) /* RxNoBuffersToPool*/
#define MVGBE_PMI_TREQ(q)		(1 << ((q) + 24)) /* TxResourceErrorQ */

/* Port Interrupt Enable (MVGBE_PIE) */
#define MVGBE_PIE_RXPKTINTRPTENB(q)	(1 << ((q) + 0))
#define MVGBE_PIE_TXPKTINTRPTENB(q)	(1 << ((q) + 8))

/* Power and PLL Control (MVGBE_PPLLC) */
#define MVGBE_PPLLC_REF_FREF_SEL_MASK	(0xf << 0)
#define MVGBE_PPLLC_PHY_MODE_MASK	(7 << 5)
#define MVGBE_PPLLC_PHY_MODE_SATA	(0 << 5)
#define MVGBE_PPLLC_PHY_MODE_SAS	(1 << 5)
#define MVGBE_PPLLC_PLL_LOCK		(1 << 8)
#define MVGBE_PPLLC_PU_DFE		(1 << 10)
#define MVGBE_PPLLC_PU_TX_INTP		(1 << 11)
#define MVGBE_PPLLC_PU_TX		(1 << 12)
#define MVGBE_PPLLC_PU_RX		(1 << 13)
#define MVGBE_PPLLC_PU_PLL		(1 << 14)

/* Digital Loopback Enable (MVGBE_DLE) */
#define MVGBE_DLE_LOCAL_SEL_BITS_MASK	(3 << 10)
#define MVGBE_DLE_LOCAL_SEL_BITS_10BITS	(0 << 10)
#define MVGBE_DLE_LOCAL_SEL_BITS_20BITS	(1 << 10)
#define MVGBE_DLE_LOCAL_SEL_BITS_40BITS	(2 << 10)
#define MVGBE_DLE_LOCAL_RXPHER_TO_TX_EN	(1 << 12)
#define MVGBE_DLE_LOCAL_ANA_TX2RX_LPBK_EN (1 << 13)
#define MVGBE_DLE_LOCAL_DIG_TX2RX_LPBK_EN (1 << 14)
#define MVGBE_DLE_LOCAL_DIG_RX2TX_LPBK_EN (1 << 15)

/* Reference Clock Select (MVGBE_RCS) */
#define MVGBE_RCS_REFCLK_SEL		(1 << 10)


/*
 * Set the chip's packet size limit to 9022.
 * (ETHER_MAX_LEN_JUMBO + ETHER_VLAN_ENCAP_LEN)
 */
#define MVGBE_MRU		9022

#define MVGBE_RXBUF_ALIGN	32	/* Cache line size */
#define MVGBE_RXBUF_MASK	(MVGBE_RXBUF_ALIGN - 1)
#define MVGBE_HWHEADER_SIZE	2


/*
 * DMA descriptors
 *    Despite the documentation saying these descriptors only need to be
 *    aligned to 16-byte bondaries, 32-byte alignment seems to be required
 *    by the hardware.  We'll just pad them out to that to make it easier.
 */
struct mvgbe_tx_desc {
#if BYTE_ORDER == BIG_ENDIAN
	uint16_t bytecnt;		/* Descriptor buffer byte count */
	uint16_t l4ichk;		/* CPU provided TCP Checksum */
	uint32_t cmdsts;		/* Descriptor command status */
	uint32_t nextdescptr;		/* Next descriptor pointer */
	uint32_t bufptr;		/* Descriptor buffer pointer */
#else	/* LITTLE_ENDIAN */
	uint32_t cmdsts;		/* Descriptor command status */
	uint16_t l4ichk;		/* CPU provided TCP Checksum */
	uint16_t bytecnt;		/* Descriptor buffer byte count */
	uint32_t bufptr;		/* Descriptor buffer pointer */
	uint32_t nextdescptr;		/* Next descriptor pointer */
#endif
	uint32_t _padding[4];
} __packed;

struct mvgbe_rx_desc {
#if BYTE_ORDER == BIG_ENDIAN
	uint16_t bytecnt;		/* Descriptor buffer byte count */
	uint16_t bufsize;		/* Buffer size */
	uint32_t cmdsts;		/* Descriptor command status */
	uint32_t nextdescptr;		/* Next descriptor pointer */
	uint32_t bufptr;		/* Descriptor buffer pointer */
#else	/* LITTLE_ENDIAN */
	uint32_t cmdsts;		/* Descriptor command status */
	uint16_t bufsize;		/* Buffer size */
	uint16_t bytecnt;		/* Descriptor buffer byte count */
	uint32_t bufptr;		/* Descriptor buffer pointer */
	uint32_t nextdescptr;		/* Next descriptor pointer */
#endif
	uint32_t _padding[4];
} __packed;

#define MVGBE_ERROR_SUMMARY		(1 << 0)
#define MVGBE_BUFFER_OWNED_MASK		(1 << 31)
#define MVGBE_BUFFER_OWNED_BY_HOST	(0 << 31)
#define MVGBE_BUFFER_OWNED_BY_DMA	(1 << 31)

#define MVGBE_TX_ERROR_CODE_MASK	(3 << 1)
#define MVGBE_TX_LATE_COLLISION_ERROR	(0 << 1)
#define MVGBE_TX_UNDERRUN_ERROR		(1 << 1)
#define MVGBE_TX_EXCESSIVE_COLLISION_ERRO (2 << 1)
#define MVGBE_TX_LLC_SNAP_FORMAT	(1 << 9)
#define MVGBE_TX_IP_NO_FRAG		(1 << 10)
#define MVGBE_TX_IP_HEADER_LEN(len)	((len) << 11)
#define MVGBE_TX_VLAN_TAGGED_FRAME	(1 << 15)
#define MVGBE_TX_L4_TYPE_TCP		(0 << 16)
#define MVGBE_TX_L4_TYPE_UDP		(1 << 16)
#define MVGBE_TX_GENERATE_L4_CHKSUM	(1 << 17)
#define MVGBE_TX_GENERATE_IP_CHKSUM	(1 << 18)
#define MVGBE_TX_ZERO_PADDING		(1 << 19)
#define MVGBE_TX_LAST_DESC		(1 << 20)
#define MVGBE_TX_FIRST_DESC		(1 << 21)
#define MVGBE_TX_GENERATE_CRC		(1 << 22)
#define MVGBE_TX_ENABLE_INTERRUPT	(1 << 23)
#define MVGBE_TX_AUTO_MODE		(1 << 30)

#define MVGBE_RX_ERROR_CODE_MASK	(3 << 1)
#define MVGBE_RX_CRC_ERROR		(0 << 1)
#define MVGBE_RX_OVERRUN_ERROR		(1 << 1)
#define MVGBE_RX_MAX_FRAME_LEN_ERROR	(2 << 1)
#define MVGBE_RX_RESOURCE_ERROR		(3 << 1)
#define MVGBE_RX_L4_CHECKSUM_MASK	(0xffff << 3)
#define MVGBE_RX_VLAN_TAGGED_FRAME	(1 << 19)
#define MVGBE_RX_BPDU_FRAME		(1 << 20)
#define MVGBE_RX_L4_TYPE_MASK		(3 << 21)
#define MVGBE_RX_L4_TYPE_TCP		(0 << 21)
#define MVGBE_RX_L4_TYPE_UDP		(1 << 21)
#define MVGBE_RX_L4_TYPE_OTHER		(2 << 21)
#define MVGBE_RX_NOT_LLC_SNAP_FORMAT	(1 << 23)
#define MVGBE_RX_IP_FRAME_TYPE		(1 << 24)
#define MVGBE_RX_IP_HEADER_OK		(1 << 25)
#define MVGBE_RX_LAST_DESC		(1 << 26)
#define MVGBE_RX_FIRST_DESC		(1 << 27)
#define MVGBE_RX_UNKNOWN_DA		(1 << 28)
#define MVGBE_RX_ENABLE_INTERRUPT	(1 << 29)
#define MVGBE_RX_L4_CHECKSUM_OK		(1 << 30)

#define MVGBE_RX_IP_FRAGMENT		(1 << 2)

#endif	/* _MVGEREG_H_ */
