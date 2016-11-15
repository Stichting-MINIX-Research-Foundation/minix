/*	$NetBSD: if_sipreg.h,v 1.19 2008/04/28 20:23:55 martin Exp $	*/

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

/*-
 * Copyright (c) 1999 Network Computer, Inc.
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
 * 3. Neither the name of Network Computer, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NETWORK COMPUTER, INC. AND CONTRIBUTORS
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

#ifndef _DEV_PCI_IF_SIPREG_H_
#define	_DEV_PCI_IF_SIPREG_H_

/*
 * Register description for the Silicon Integrated Systems SiS 900,
 * SiS 7016, National Semiconductor DP83815 10/100, and National
 * Semiconduction DP83820 10/100/1000 PCI Ethernet controller.
 *
 * Written by Jason R. Thorpe for Network Computer, Inc.
 */

/*
 * Transmit FIFO size.  Used to compute the transmit drain threshold.
 *
 * On the SiS 900, the transmit FIFO is arranged as a 512 32-bit memory
 * array.
 *
 * On the DP83820, we have an 8KB transmit FIFO.
 */
#define	DP83820_SIP_TXFIFO_SIZE	8192
#define	OTHER_SIP_TXFIFO_SIZE	(512 * 4)

/*
 * The SiS900 uses a single descriptor format for both transmit
 * and receive descriptor chains.
 *
 * Note the DP83820 can use 64-bit DMA addresses for link and bufptr.
 * However, we do not yet support that.
 *
 * For transmit, buffers need not be aligned.  For receive, buffers
 * must be aligned to 4-byte (8-byte on DP83820) boundaries.
 */
struct sip_desc {
	u_int32_t	sipd_link;	/* link to next descriptor */
	uint32_t	sipd_cbs[2];	/* command/status and pointer to
					 * DMA segment
					 */
	u_int32_t	sipd_extsts;	/* extended status */
};

/*
 * CMDSTS bits common to transmit and receive.
 */
#define	CMDSTS_OWN	0x80000000	/* owned by consumer */
#define	CMDSTS_MORE	0x40000000	/* more descriptors */
#define	CMDSTS_INTR	0x20000000	/* interrupt when ownership changes */
#define	CMDSTS_SUPCRC	0x10000000	/* suppress CRC */
#define	CMDSTS_OK	0x08000000	/* packet ok */
#define	DP83820_CMDSTS_SIZE_MASK 0x0000ffff	/* packet size */
#define	OTHER_CMDSTS_SIZE_MASK 0x000007ff	/* packet size */

#define	CMDSTS_SIZE(sc, x)	((x) & sc->sc_bits.b_cmdsts_size_mask)

/*
 * CMDSTS bits for transmit.
 */
#define	CMDSTS_Tx_TXA	0x04000000	/* transmit abort */
#define	CMDSTS_Tx_TFU	0x02000000	/* transmit FIFO underrun */
#define	CMDSTS_Tx_CRS	0x01000000	/* carrier sense lost */
#define	CMDSTS_Tx_TD	0x00800000	/* transmit deferred */
#define	CMDSTS_Tx_ED	0x00400000	/* excessive deferral */
#define	CMDSTS_Tx_OWC	0x00200000	/* out of window collision */
#define	CMDSTS_Tx_EC	0x00100000	/* excessive collisions */
#define	CMDSTS_Tx_CCNT	0x000f0000	/* collision count */

#define	CMDSTS_COLLISIONS(x)	(((x) & CMDSTS_Tx_CCNT) >> 16)

/*
 * CMDSTS bits for receive.
 */
#define	CMDSTS_Rx_RXA	0x04000000	/* receive abort */
#define	CMDSTS_Rx_RXO	0x02000000	/* receive overrun */
#define	CMDSTS_Rx_DEST	0x01800000	/* destination class */
#define	CMDSTS_Rx_LONG	0x00400000	/* packet too long */
#define	CMDSTS_Rx_RUNT	0x00200000	/* runt packet */
#define	CMDSTS_Rx_ISE	0x00100000	/* invalid symbol error */
#define	CMDSTS_Rx_CRCE	0x00080000	/* CRC error */
#define	CMDSTS_Rx_FAE	0x00040000	/* frame alignment error */
#define	CMDSTS_Rx_LBP	0x00020000	/* loopback packet */
/* #ifdef DP83820 */
#define	CMDSTS_Rx_IRL	0x00010000	/* in-range length error */
/* #else */
#define	CMDSTS_Rx_COL	0x00010000	/* collision activity */
/* #endif DP83820 */

#define	CMDSTS_Rx_DEST_REJ 0x00000000	/* packet rejected */
#define	CMDSTS_Rx_DEST_STA 0x00800000	/* matched station address */
#define	CMDSTS_Rx_DEST_MUL 0x01000000	/* multicast address */
#define	CMDSTS_Rx_DEST_BRD 0x01800000	/* broadcast address */

/*
 * EXTSTS bits.
 */
#define	EXTSTS_Rx_UDPERR 0x00400000	/* UDP checksum error */
#define	EXTSTS_UDPPKT	 0x00200000	/* perform UDP checksum */
#define	EXTSTS_Rx_TCPERR 0x00100000	/* TCP checksum error */
#define	EXTSTS_TCPPKT	 0x00080000	/* perform TCP checksum */
#define	EXTSTS_Rx_IPERR	 0x00040000	/* IP header checksum error */
#define	EXTSTS_IPPKT	 0x00020000	/* perform IP header checksum */
#define	EXTSTS_VPKT	 0x00010000	/* insert VLAN tag */
#define	EXTSTS_VTCI	 0x0000ffff	/* VLAN tag control information */

/*
 * PCI Configuration space registers.
 */
#define	SIP_PCI_CFGIOA	(PCI_MAPREG_START + 0x00)

#define	SIP_PCI_CFGMA	(PCI_MAPREG_START + 0x04)

/* DP83820 only */
#define	SIP_PCI_CFGMA1	(PCI_MAPREG_START + 0x08)

#define	SIP_PCI_CFGEROMA 0x30		/* expansion ROM address */

#define	SIP_PCI_CFGPMC	 0x40		/* power management cap. */

#define	SIP_PCI_CFGPMCSR 0x44		/* power management ctl. */

/*
 * MAC Operation Registers
 */
#define	SIP_CR		0x00	/* command register */

/* DP83820 only */
#define	CR_RXPRI3	0x00010000	/* Rx priority queue select */
#define	CR_RXPRI2	0x00008000	/* Rx priority queue select */
#define	CR_RXPRI1	0x00004000	/* Rx priority queue select */
#define	CR_RXPRI0	0x00002000	/* Rx priority queue select */
#define	CR_TXPRI3	0x00001000	/* Tx priority queue select */
#define	CR_TXPRI2	0x00000800	/* Tx priority queue select */
#define	CR_TXPRI1	0x00000400	/* Tx priority queue select */
#define	CR_TXPRI0	0x00000200	/* Tx priority queue select */

#define	CR_RLD		0x00000400	/* reload from NVRAM */
#define	CR_RST		0x00000100	/* software reset */
#define	CR_SWI		0x00000080	/* software interrupt */
#define	CR_RXR		0x00000020	/* receiver reset */
#define	CR_TXR		0x00000010	/* transmit reset */
#define	CR_RXD		0x00000008	/* receiver disable */
#define	CR_RXE		0x00000004	/* receiver enable */
#define	CR_TXD		0x00000002	/* transmit disable */
#define	CR_TXE		0x00000001	/* transmit enable */

#define	SIP_CFG		0x04	/* configuration register */
#define	CFG_LNKSTS	0x80000000	/* link status (83815) */
/* #ifdef DP83820 */
#define	CFG_SPEED1000	0x40000000	/* 1000Mb/s input pin */
#define	CFG83820_SPEED100	0x20000000	/* 100Mb/s input pin */
#define	CFG_DUPSTS	0x10000000	/* full-duplex status */
#define	CFG_TBI_EN	0x01000000	/* ten-bit interface enable */
#define	CFG_MODE_1000	0x00400000	/* 1000Mb/s mode enable */
#define	CFG_PINT_DUP	0x00100000	/* interrupt on PHY DUP change */
#define	CFG_PINT_LNK	0x00080000	/* interrupt on PHY LNK change */
#define	CFG_PINT_SPD	0x00040000	/* interrupt on PHY SPD change */
#define	CFG_TMRTEST	0x00020000	/* timer test mode */
#define	CFG_MRM_DIS	0x00010000	/* MRM disable */
#define	CFG_MWI_DIS	0x00008000	/* MWI disable */
#define	CFG_T64ADDR	0x00004000	/* target 64-bit addressing enable */
#define	CFG_PCI64_DET	0x00002000	/* 64-bit PCI bus detected */
#define	CFG_DATA64_EN	0x00001000	/* 64-bit data enable */
#define	CFG_M64ADDR	0x00000800	/* master 64-bit addressing enable */
/* #else */
#define	CFG83815_SPEED100	0x40000000	/* 100Mb/s (83815) */
#define	CFG_FDUP	0x20000000	/* full duplex (83815) */
#define	CFG_POL		0x10000000	/* 10Mb/s polarity (83815) */
#define	CFG_ANEG_DN	0x08000000	/* autonegotiation done (83815) */
#define	CFG_PHY_CFG	0x00fc0000	/* PHY configuration (83815) */
#define	CFG_PINT_ACEN	0x00020000	/* PHY interrupt auto clear (83815) */
#define	CFG_PAUSE_ADV	0x00010000	/* pause advertise (83815) */
#define	CFG_ANEG_SEL	0x0000e000	/* autonegotiation select (83815) */
/* #endif DP83820 */
#define	CFG_PHY_RST	0x00000400	/* PHY reset (83815) */
#define	CFG_PHY_DIS	0x00000200	/* PHY disable (83815) */
/* #ifdef DP83820 */
#define	CFG_EXTSTS_EN	0x00000100	/* extended status enable */
/* #else */
#define	CFG_EUPHCOMP	0x00000100	/* 83810 descriptor compat (83815) */
/* #endif DP83820 */
#define	CFG_EDBMASTEN	0x00002000	/* 635,900B ?? from linux driver */
#define	CFG_RNDCNT	0x00000400	/* 635,900B ?? from linux driver */
#define	CFG_FAIRBO	0x00000200	/* 635,900B ?? from linux driver */
#define	CFG_REQALG	0x00000080	/* PCI bus request alg. */
#define	CFG_SB		0x00000040	/* single backoff */
#define	CFG_POW		0x00000020	/* program out of window timer */
#define	CFG_EXD		0x00000010	/* excessive defferal timer disable */
#define	CFG_PESEL	0x00000008	/* parity error detection action */
/* #ifdef DP83820 */
#define	CFG_BROM_DIS	0x00000004	/* boot ROM disable */
#define	CFG_EXT_125	0x00000002	/* external 125MHz reference select */
/* #endif DP83820 */
#define	CFG_BEM		0x00000001	/* big-endian mode */

#define	SIP_EROMAR	0x08	/* EEPROM access register */
#define	EROMAR_REQ	0x00000400	/* SiS 96x specific */
#define	EROMAR_DONE	0x00000200	/* SiS 96x specific */
#define	EROMAR_GNT	0x00000100	/* SiS 96x specific */
#define	EROMAR_MDC	0x00000040	/* MII clock */
#define	EROMAR_MDDIR	0x00000020	/* MII direction (1 == MAC->PHY) */
#define	EROMAR_MDIO	0x00000010	/* MII data */
#define	EROMAR_EECS	0x00000008	/* chip select */
#define	EROMAR_EESK	0x00000004	/* clock */
#define	EROMAR_EEDO	0x00000002	/* data out */
#define	EROMAR_EEDI	0x00000001	/* data in */

#define	SIP_PTSCR	0x0c	/* PCI test control register */
#define	PTSCR_RBIST_RST	    0x00002000	/* SRAM BIST reset */
#define	PTSCR_RBIST_EN	    0x00000400	/* SRAM BIST enable */
#define	PTSCR_RBIST_DONE    0x00000200	/* SRAM BIST done */
#define	PTSCR_RBIST_RX1FAIL 0x00000100	/* Rx status FIFO BIST fail */
#define	PTSCR_RBIST_RX0FAIL 0x00000080	/* Rx data FIFO BIST fail */
#define	PTSCR_RBIST_TX0FAIL 0x00000020	/* Tx data FIFO BIST fail */
#define	PTSCR_RBIST_HFFAIL  0x00000010	/* hash filter BIST fail */
#define	PTSCR_RBIST_RXFAIL  0x00000008	/* Rx filter BIST failed */
#define	PTSCR_EELOAD_EN	    0x00000004	/* EEPROM load initiate */
#define	PTSCR_EEBIST_EN	    0x00000002	/* EEPROM BIST enable */
#define	PTSCR_EEBIST_FAIL   0x00000001	/* EEPROM BIST failed */
#define	PTSCR_DIS_TEST	0x40000000	/* discard timer test mode */
#define	PTSCR_EROM_TACC	0x0f000000	/* boot rom access time */
#define	PTSCR_TRRAMADR	0x001ff000	/* TX/RX RAM address */
#define	PTSCR_BMTEN	0x00000200	/* bus master test enable */
#define	PTSCR_RRTMEN	0x00000080	/* receive RAM test mode enable */
#define	PTSCR_TRTMEN	0x00000040	/* transmit RAM test mode enable */
#define	PTSCR_SRTMEN	0x00000020	/* status RAM test mode enable */
#define	PTSCR_SRAMADR	0x0000001f	/* status RAM address */

#define	SIP_ISR		0x10	/* interrupt status register */
/* DP83820 only */
#define	ISR_TXDESC3	0x40000000	/* Tx queue 3 */
#define	ISR_TXDESC2	0x20000000	/* Tx queue 2 */
#define	ISR_TXDESC1	0x10000000	/* Tx queue 1 */
#define	ISR_TXDESC0	0x08000000	/* Tx queue 0 */
#define	ISR_RXDESC3	0x04000000	/* Rx queue 3 */
#define	ISR_RXDESC2	0x02000000	/* Rx queue 2 */
#define	ISR_RXDESC1	0x01000000	/* Rx queue 1 */
#define	ISR_RXDESC0	0x00800000	/* Rx queue 0 */

/* non-DP83820 only */
#define	ISR_WAKEEVT	0x10000000	/* wake up event */

#if 0
#ifdef DP83820
#define	ISR_TXRCMP	0x00400000	/* transmit reset complete */
#define	ISR_RXRCMP	0x00200000	/* receive reset complete */
#define	ISR_DPERR	0x00100000	/* detected parity error */
#define	ISR_SSERR	0x00080000	/* signalled system error */
#define	ISR_RMABT	0x00040000	/* received master abort */
#define	ISR_RTABT	0x00020000	/* received target abort */
#else
#define	ISR_TXRCMP	0x02000000	/* transmit reset complete */
#define	ISR_RXRCMP	0x01000000	/* receive reset complete */
#define	ISR_DPERR	0x00800000	/* detected parity error */
#define	ISR_SSERR	0x00400000	/* signalled system error */
#define	ISR_RMABT	0x00200000	/* received master abort */
#define	ISR_RTABT	0x00100000	/* received target abort */
#endif /* DP83820 */
#endif /* 0 */

/* SiS 900 only */
#define	ISR_PAUSE_END	0x08000000	/* end of transmission pause */
#define	ISR_PAUSE_ST	0x04000000	/* start of transmission pause */

#define	ISR_RXSOVR	0x00010000	/* Rx status FIFO overrun */
#define	ISR_HIBERR	0x00008000	/* high bits error set */

/* DP83820 only */
#define	ISR_PHY		0x00004000	/* PHY interrupt */
#define	ISR_PME		0x00002000	/* power management event */

#define	ISR_SWI		0x00001000	/* software interrupt */

/* DP83820 only */
#define	ISR_MIB		0x00000800	/* MIB service */

#define	ISR_TXURN	0x00000400	/* Tx underrun */
#define	ISR_TXIDLE	0x00000200	/* Tx idle */
#define	ISR_TXERR	0x00000100	/* Tx error */
#define	ISR_TXDESC	0x00000080	/* Tx descriptor interrupt */
#define	ISR_TXOK	0x00000040	/* Tx okay */
#define	ISR_RXORN	0x00000020	/* Rx overrun */
#define	ISR_RXIDLE	0x00000010	/* Rx idle */
#define	ISR_RXEARLY	0x00000008	/* Rx early */
#define	ISR_RXERR	0x00000004	/* Rx error */
#define	ISR_RXDESC	0x00000002	/* Rx descriptor interrupt */
#define	ISR_RXOK	0x00000001	/* Rx okay */

#define	SIP_IMR		0x14	/* interrupt mask register */
/* See bits in SIP_ISR */

#define	SIP_IER		0x18	/* interrupt enable register */
#define	IER_IE		0x00000001	/* master interrupt enable */

/* #ifdef DP83820 */
#define	SIP_IHR		0x1c	/* interrupt hold-off register */
#define	IHR_IHCTL	0x00000100	/* interrupt hold-off control */
#define	IHR_IH		0x000000ff	/* interrupt hold-off timer (100us) */
/* #else */
#define	SIP_ENPHY	0x1c	/* enhanced PHY access register */
#define	ENPHY_PHYDATA	0xffff0000	/* PHY data */
#define	ENPHY_DATA_SHIFT 16
#define	ENPHY_PHYADDR	0x0000f800	/* PHY number (7016 only) */
#define	ENPHY_PHYADDR_SHIFT 11
#define	ENPHY_REGADDR	0x000007c0	/* PHY register */
#define	ENPHY_REGADDR_SHIFT 6
#define	ENPHY_RWCMD	0x00000020	/* 1 == read, 0 == write */
#define	ENPHY_ACCESS	0x00000010	/* PHY access enable */
/* #endif DP83820 */

#define	SIP_TXDP	0x20	/* transmit descriptor pointer reg */

/* DP83820 only */
#define	SIP_TXDP_HI	0x24	/* transmit descriptor pointer (high) reg */

#define	DP83820_SIP_TXCFG	0x28	/* transmit configuration register */
#define	OTHER_SIP_TXCFG	0x24	/* transmit configuration register */

#define	TXCFG_CSI	0x80000000	/* carrier sense ignore */
#define	TXCFG_HBI	0x40000000	/* heartbeat ignore */
#define	TXCFG_MLB	0x20000000	/* MAC loopback */
#define	TXCFG_ATP	0x10000000	/* automatic transmit padding */
#define	TXCFG_MXDMA	0x00700000	/* max DMA burst size */

/* DP83820 only */
#define	TXCFG_ECRETRY	0x008000000	/* excessive collision retry enable */
#define	TXCFG_BRST_DIS	0x00080000	/* 1000Mb/s burst disable */

/* DP83820 only */
#define	TXCFG_MXDMA_1024 0x00000000	/*    1024 bytes */
#if 0
#ifdef DP83820
#define	TXCFG_MXDMA_8	 0x00100000	/*       8 bytes */
#define	TXCFG_MXDMA_16	 0x00200000	/*      16 bytes */
#define	TXCFG_MXDMA_32	 0x00300000	/*      32 bytes */
#define	TXCFG_MXDMA_64	 0x00400000	/*      64 bytes */
#define	TXCFG_MXDMA_128	 0x00500000	/*     128 bytes */
#define	TXCFG_MXDMA_256	 0x00600000	/*     256 bytes */
#define	TXCFG_MXDMA_512	 0x00700000	/*     512 bytes */
#define	TXCFG_FLTH_MASK	0x0000ff00	/* Fx fill threshold */
#define	TXCFG_DRTH_MASK	0x000000ff	/* Tx drain threshold */
#else
#define	TXCFG_MXDMA_512	0x00000000	/*     512 bytes */
#define	TXCFG_MXDMA_8	0x00200000	/*       8 bytes */
#define	TXCFG_MXDMA_16	0x00300000	/*      16 bytes */
#define	TXCFG_MXDMA_32	0x00400000	/*      32 bytes */
#define	TXCFG_MXDMA_64	0x00500000	/*      64 bytes */
#define	TXCFG_MXDMA_128	0x00600000	/*     128 bytes */
#define	TXCFG_MXDMA_256	0x00700000	/*     256 bytes */
#define	TXCFG_FLTH_MASK	0x00003f00	/* Tx fill threshold */
#define	TXCFG_DRTH_MASK	0x0000003f	/* Tx drain threshold */
#endif /* DP83820 */
#endif /* 0 */

/* non-DP83820 only */
#define	TXCFG_MXDMA_4	0x00100000	/*       4 bytes */

#define	SIP_GPIOR	0x2c	/* general purpose i/o register */
#define	GPIOR_GP5_IN	0x00004000	/* GP 5 in */
#define	GPIOR_GP4_IN	0x00002000	/* GP 4 in */
#define	GPIOR_GP3_IN	0x00001000	/* GP 3 in */
#define	GPIOR_GP2_IN	0x00000800	/* GP 2 in */
#define	GPIOR_GP1_IN	0x00000400	/* GP 1 in */
#define	GPIOR_GP5_OE	0x00000200	/* GP 5 out enable */
#define	GPIOR_GP4_OE	0x00000100	/* GP 4 out enable */
#define	GPIOR_GP3_OE	0x00000080	/* GP 3 out enable */
#define	GPIOR_GP2_OE	0x00000040	/* GP 2 out enable */
#define	GPIOR_GP1_OE	0x00000020	/* GP 1 out enable */
#define	GPIOR_GP5_OUT	0x00000010	/* GP 5 out */
#define	GPIOR_GP4_OUT	0x00000008	/* GP 4 out */
#define	GPIOR_GP3_OUT	0x00000004	/* GP 3 out */
#define	GPIOR_GP2_OUT	0x00000002	/* GP 2 out */
#define	GPIOR_GP1_OUT	0x00000001	/* GP 1 out */

#define	SIP_RXDP	0x30	/* receive descriptor pointer reg */

/* DP83820 only */
#define	SIP_RXDP_HI	0x34	/* receive descriptor pointer (high) reg */

#define	DP83820_SIP_RXCFG	0x38	/* receive configuration register */
#define	OTHER_SIP_RXCFG	0x34	/* receive configuration register */
#define	RXCFG_AEP	0x80000000	/* accept error packets */
#define	RXCFG_ARP	0x40000000	/* accept runt packets */
/* DP83820 only */
#define	RXCFG_STRIPCRC	0x20000000	/* strip CRC */

#define	RXCFG_ATX	0x10000000	/* accept transmit packets */
#define	RXCFG_ALP	0x08000000	/* accept long packets */

/* DP83820 only */
#define	RXCFG_AIRL	0x04000000	/* accept in-range length err packets */

#define	RXCFG_MXDMA	 0x00700000	/* max DMA burst size */

/* DP83820 only */
#define	RXCFG_MXDMA_1024 0x00000000	/*    1024 bytes */

#if 0
#ifdef DP83820
#define	RXCFG_MXDMA_8	 0x00100000	/*       8 bytes */
#define	RXCFG_MXDMA_16	 0x00200000	/*      16 bytes */
#define	RXCFG_MXDMA_32	 0x00300000	/*      32 bytes */
#define	RXCFG_MXDMA_64	 0x00400000	/*      64 bytes */
#define	RXCFG_MXDMA_128	 0x00500000	/*     128 bytes */
#define	RXCFG_MXDMA_256	 0x00600000	/*     256 bytes */
#define	RXCFG_MXDMA_512	 0x00700000	/*     512 bytes */
#else
#define	RXCFG_MXDMA_512	0x00000000	/*     512 bytes */
#define	RXCFG_MXDMA_8	0x00200000	/*       8 bytes */
#define	RXCFG_MXDMA_16	0x00300000	/*      16 bytes */
#define	RXCFG_MXDMA_32	0x00400000	/*      32 bytes */
#define	RXCFG_MXDMA_64	0x00500000	/*      64 bytes */
#define	RXCFG_MXDMA_128	0x00600000	/*     128 bytes */
#define	RXCFG_MXDMA_256	0x00700000	/*     256 bytes */
#endif /* DP83820 */
#endif /* 0 */

/* non-DP83820 only */
#define	RXCFG_MXDMA_4	0x00100000	/*       4 bytes */
#define	RXCFG_DRTH_MASK	0x0000003e

/* DP83820 only */
#define	SIP_PQCR	0x3c	/* priority queueing control register */
#define	PQCR_RXPQ_4	0x0000000c	/* 4 Rx queues */
#define	PQCR_RXPQ_3	0x00000008	/* 3 Rx queues */
#define	PQCR_RXPQ_2	0x00000004	/* 2 Rx queues */
#define	PQCR_TXFAIR	0x00000002	/* Tx fairness enable */
#define	PQCR_TXPQEN	0x00000001	/* Tx priority queueing enable */

/* DP83815 only */
#define	SIP83815_NS_CCSR	0x3c	/* CLKRUN control/status register (83815) */
#define	CCSR_PMESTS	0x00008000	/* PME status */
#define	CCSR_PMEEN	0x00000100	/* PME enable */
#define	CCSR_CLKRUN_EN	0x00000001	/* clkrun enable */

/* SiS 900 only */
#define	SIP_FLOWCTL	0x38	/* flow control register */
#define	FLOWCTL_PAUSE	0x00000002	/* PAUSE flag */
#define	FLOWCTL_FLOWEN	0x00000001	/* enable flow control */

#define	SIP_NS_WCSR	0x40	/* WoL control/status register (83815/83820) */

#define	SIP_NS_PCR	0x44	/* pause control/status reg (83815/83820) */
#define	PCR_PSEN	0x80000000 /* pause enable */
#define	PCR_PS_MCAST	0x40000000 /* pause on multicast */
#define	PCR_PS_DA	0x20000000 /* pause on DA */
#define	PCR_PS_ACT	0x10000000 /* pause active */
#define	PCR_PS_RCVD	0x08000000 /* pause packet recieved */
/* #ifdef DP83820 */
#define	PCR_PS_STHI_8	0x03000000 /* Status FIFO Hi Threshold (8packets) */
#define	PCR_PS_STHI_4	0x02000000 /* Status FIFO Hi Threshold (4packets) */
#define	PCR_PS_STHI_2	0x01000000 /* Status FIFO Hi Threshold (2packets) */
#define	PCR_PS_STHI_0	0x00000000 /* Status FIFO Hi Threshold (disable) */
#define	PCR_PS_STLO_8	0x00c00000 /* Status FIFO Lo Threshold (8packets) */
#define	PCR_PS_STLO_4	0x00800000 /* Status FIFO Lo Threshold (4packets) */
#define	PCR_PS_STLO_2	0x00400000 /* Status FIFO Lo Threshold (2packets) */
#define	PCR_PS_STLO_0	0x00000000 /* Status FIFO Lo Threshold (disable) */
#define	PCR_PS_FFHI_8	0x00300000 /* Data FIFO Hi Threshold (8Kbyte) */
#define	PCR_PS_FFHI_4	0x00200000 /* Data FIFO Hi Threshold (4Kbyte) */
#define	PCR_PS_FFHI_2	0x00100000 /* Data FIFO Hi Threshold (2Kbyte) */
#define	PCR_PS_FFHI_0	0x00000000 /* Data FIFO Hi Threshold (disable) */
#define	PCR_PS_FFLO_8	0x000c0000 /* Data FIFO Lo Threshold (8Kbyte) */
#define	PCR_PS_FFLO_4	0x00080000 /* Data FIFO Lo Threshold (4Kbyte) */
#define	PCR_PS_FFLO_2	0x00040000 /* Data FIFO Lo Threshold (2Kbyte) */
#define	PCR_PS_FFLO_0	0x00000000 /* Data FIFO Lo Threshold (disable) */
#define	PCR_PS_TX	0x00020000 /* Transmit PAUSE frame manually */
/* #else */
#define	PCR_PSNEG	0x00200000 /* Pause Negoticated (83815) */
#define	PCR_MLD_EN	0x00010000 /* Manual Load Enable (83815) */
/* #endif DP83820 */
#define PCR_PAUSE_CNT_MASK 0x0000ffff /* pause count mask */
#define PCR_PAUSE_CNT	   65535      /* pause count (512bit-time) */

#define	SIP_RFCR	0x48	/* receive filter control register */
#define	RFCR_RFEN	0x80000000	/* Rx filter enable */
#define	RFCR_AAB	0x40000000	/* accept all broadcast */
#define	RFCR_AAM	0x20000000	/* accept all multicast */
#define	RFCR_AAP	0x10000000	/* accept all physical */
#define	RFCR_APM	0x08000000	/* accept perfect match (83815) */
#define	RFCR_APAT	0x07800000	/* accept pattern match (83815) */
#define	RFCR_AARP	0x00400000	/* accept ARP (83815) */
#define	RFCR_MHEN	0x00200000	/* multicast hash enable (83815) */
#define	RFCR_UHEN	0x00100000	/* unicast hash enable (83815) */
#define	RFCR_ULM	0x00080000	/* U/L bit mask (83815) */
#define	RFCR_NS_RFADDR	0x000003ff	/* Rx filter ext reg address (83815) */
#define	RFCR_RFADDR	0x000f0000	/* Rx filter address */
#define	RFCR_RFADDR_NODE0 0x00000000	/* node address 1, 0 */
#define	RFCR_RFADDR_NODE2 0x00010000	/* node address 3, 2 */
#define	RFCR_RFADDR_NODE4 0x00020000	/* node address 5, 4 */
#define	RFCR_RFADDR_MC0	  0x00040000	/* multicast hash word 0 */
#define	RFCR_RFADDR_MC1	  0x00050000	/* multicast hash word 1 */
#define	RFCR_RFADDR_MC2	  0x00060000	/* multicast hash word 2 */
#define	RFCR_RFADDR_MC3	  0x00070000	/* multicast hash word 3 */
#define	RFCR_RFADDR_MC4	  0x00080000	/* multicast hash word 4 */
#define	RFCR_RFADDR_MC5	  0x00090000	/* multicast hash word 5 */
#define	RFCR_RFADDR_MC6	  0x000a0000	/* multicast hash word 6 */
#define	RFCR_RFADDR_MC7	  0x000b0000	/* multicast hash word 7 */
/* For SiS900B and 635/735 only */
#define	RFCR_RFADDR_MC8	  0x000c0000	/* multicast hash word 8 */
#define	RFCR_RFADDR_MC9	  0x000d0000	/* multicast hash word 9 */
#define	RFCR_RFADDR_MC10  0x000e0000	/* multicast hash word 10 */
#define	RFCR_RFADDR_MC11  0x000f0000	/* multicast hash word 11 */
#define	RFCR_RFADDR_MC12  0x00100000	/* multicast hash word 12 */
#define	RFCR_RFADDR_MC13  0x00110000	/* multicast hash word 13 */
#define	RFCR_RFADDR_MC14  0x00120000	/* multicast hash word 14 */
#define	RFCR_RFADDR_MC15  0x00130000	/* multicast hash word 15 */

#define	RFCR_NS_RFADDR_PMATCH0	0x0000	/* perfect match octets 1-0 */
#define	RFCR_NS_RFADDR_PMATCH2	0x0002	/* perfect match octets 3-2 */
#define	RFCR_NS_RFADDR_PMATCH4	0x0004	/* perfect match octets 5-4 */
#define	RFCR_NS_RFADDR_PCOUNT	0x0006	/* pattern count */

/* DP83820 only */
#define	RFCR_NS_RFADDR_PCOUNT2	0x0008	/* pattern count 2, 3 */
#define	RFCR_NS_RFADDR_SOPAS0	0x000a	/* SecureOn 0, 1 */
#define	RFCR_NS_RFADDR_SOPAS2	0x000c	/* SecureOn 2, 3 */
#define	RFCR_NS_RFADDR_SOPAS4	0x000e	/* SecureOn 4, 5 */
#define	RFCR_NS_RFADDR_PATMEM	0x0200	/* pattern memory */

#define	DP83820_RFCR_NS_RFADDR_FILTMEM	0x0100	/* hash memory */
#define	OTHER_RFCR_NS_RFADDR_FILTMEM	0x0200	/* filter memory (hash/pattern) */

#define	SIP_RFDR	0x4c	/* receive filter data register */
#define	RFDR_BMASK	0x00030000	/* byte mask (83815) */
#define	RFDR_DATA	0x0000ffff	/* data bits */

#define	SIP_NS_BRAR	0x50	/* boot rom address (83815) */
#define	BRAR_AUTOINC	0x80000000	/* autoincrement */
#define	BRAR_ADDR	0x0000ffff	/* address */

#define	SIP_NS_BRDR	0x54	/* boot rom data (83815) */

#define	SIP_NS_SRR	0x58	/* silicon revision register (83815) */
/* #ifdef DP83820 */
#define	SRR_REV_B	0x00000103
/* #else */
#define	SRR_REV_A	0x00000101
#define	SRR_REV_B_1	0x00000200
#define	SRR_REV_B_2	0x00000201
#define	SRR_REV_B_3	0x00000203
#define	SRR_REV_C_1	0x00000300
#define	SRR_REV_C_2	0x00000302
/* #endif DP83820 */

#define	SIP_NS_MIBC	0x5c	/* mib control register (83815) */
#define	MIBC_MIBS	0x00000008	/* mib counter strobe */
#define	MIBC_ACLR	0x00000004	/* clear all counters */
#define	MIBC_FRZ	0x00000002	/* freeze all counters */
#define	MIBC_WRN	0x00000001	/* warning test indicator */

#define	SIP_NS_MIB(mibreg)	/* mib data registers (83815) */	\
	(0x60 + (mibreg))
#define	MIB_RXErroredPkts	0x00
#define	MIB_RXFCSErrors		0x04
#define	MIB_RXMsdPktErrors	0x08
#define	MIB_RXFAErrors		0x0c
#define	MIB_RXSymbolErrors	0x10
#define	MIB_RXFrameTooLong	0x14
/* #ifdef DP83820 */
#define	MIB_RXIRLErrors		0x18
#define	MIB_RXBadOpcodes	0x1c
#define	MIB_RXPauseFrames	0x20
#define	MIB_TXPauseFrames	0x24
#define	MIB_TXSQEErrors		0x28
/* #else */
#define	MIB_RXTXSQEErrors	0x18
/* #endif DP83820 */

/* 83815 only */
#define	SIP_NS_PHY(miireg)	/* PHY registers (83815) */		\
	(0x80 + ((miireg) << 2))

/* #ifdef DP83820 */
#define	SIP_TXDP1	0xa0	/* transmit descriptor pointer (pri 1) */

#define	SIP_TXDP2	0xa4	/* transmit descriptor pointer (pri 2) */

#define	SIP_TXDP3	0xa8	/* transmit descriptor pointer (pri 3) */

#define	SIP_RXDP1	0xb0	/* receive descriptor pointer (pri 1) */

#define	SIP_RXDP2	0xb4	/* receive descriptor pointer (pri 2) */

#define	SIP_RXDP3	0xb8	/* receive descriptor pointer (pri 3) */

#define	SIP_VRCR	0xbc	/* VLAN/IP receive control register */
#define	VRCR_RUDPE	0x00000080	/* reject UDP checksum errors */
#define	VRCR_RTCPE	0x00000040	/* reject TCP checksum errors */
#define	VRCR_RIPE	0x00000020	/* reject IP checksum errors */
#define	VRCR_IPEN	0x00000010	/* IP checksum enable */
#define	VRCR_DUTF	0x00000008	/* discard untagged frames */
#define	VRCR_DVTF	0x00000004	/* discard VLAN tagged frames */
#define	VRCR_VTREN	0x00000002	/* VLAN tag removal enable */
#define	VRCR_VTDEN	0x00000001	/* VLAN tag detection enable */

#define	SIP_VTCR	0xc0	/* VLAN/IP transmit control register */
#define	VTCR_PPCHK	0x00000008	/* per-packet checksum generation */
#define	VTCR_GCHK	0x00000004	/* global checksum generation */
#define	VTCR_VPPTI	0x00000002	/* VLAN per-packet tag insertion */
#define	VTCR_VGTI	0x00000001	/* VLAN global tag insertion */

#define	SIP_VDR		0xc4	/* VLAN data register */
#define	VDR_VTCI	0xffff0000	/* VLAN tag control information */
#define	VDR_VTYPE	0x0000ffff	/* VLAN type field */

#define	SIP83820_NS_CCSR	0xcc	/* CLKRUN control/status register (83820) */
#if 0
#define	CCSR_PMESTS	0x00008000	/* PME status */
#define	CCSR_PMEEN	0x00000100	/* PME enable */
#define	CCSR_CLKRUN_EN	0x00000001	/* clkrun enable */
#endif

#define	SIP_TBICR	0xe0	/* TBI control register */
#define	TBICR_MR_LOOPBACK   0x00004000	/* TBI PCS loopback enable */
#define	TBICR_MR_AN_ENABLE  0x00001000	/* TBI autonegotiation enable */
#define	TBICR_MR_RESTART_AN 0x00000200	/* restart TBI autoneogtiation */

#define	SIP_TBISR	0xe4	/* TBI status register */
#define	TBISR_MR_LINK_STATUS 0x00000020	/* TBI link status */
#define	TBISR_MR_AN_COMPLETE 0x00000004	/* TBI autonegotiation complete */

#define	SIP_TANAR	0xe8	/* TBI autoneg adv. register */
#define	TANAR_NP	0x00008000	/* next page exchange required */
#define	TANAR_RF2	0x00002000	/* remote fault 2 */
#define	TANAR_RF1	0x00001000	/* remote fault 1 */
#define	TANAR_PS2	0x00000100	/* pause encoding 2 */
#define	TANAR_PS1	0x00000080	/* pause encoding 1 */
#define	TANAR_HALF_DUP	0x00000040	/* adv. half duplex */
#define	TANAR_FULL_DUP	0x00000020	/* adv. full duplex */

#define	SIP_TANLPAR	0xec	/* TBI autoneg link partner ability register */
	/* See TANAR bits */

#define	SIP_TANER	0xf0	/* TBI autoneg expansion register */
#define	TANER_NPA	0x00000004	/* we support next page function */
#define	TANER_PR	0x00000002	/* page received from link partner */

#define	SIP_TESR	0xf4	/* TBI extended status register */
#define	TESR_1000FDX	0x00008000	/* we support 1000base FDX */
#define	TESR_1000HDX	0x00004000	/* we support 1000base HDX */
/* #else */
#define	SIP_PMCTL	0xb0	/* power management control register */
#define	PMCTL_GATECLK	0x80000000	/* gate dual clock enable */
#define	PMCTL_WAKEALL	0x40000000	/* wake on all Rx OK */
#define	PMCTL_FRM3ACS	0x04000000	/* 3rd wake-up frame access */
#define	PMCTL_FRM2ACS	0x02000000	/* 2nd wake-up frame access */
#define	PMCTL_FRM1ACS	0x01000000	/* 1st wake-up frame access */
#define	PMCTL_FRM3EN	0x00400000	/* 3rd wake-up frame match enable */
#define	PMCTL_FRM2EN	0x00200000	/* 2nd wake-up frame match enable */
#define	PMCTL_FRM1EN	0x00100000	/* 1st wake-up frame match enable */
#define	PMCTL_ALGORITHM	0x00000800	/* Magic Packet match algorithm */
#define	PMCTL_MAGICPKT	0x00000400	/* Magic Packet match enable */
#define	PMCTL_LINKON	0x00000002	/* link on monitor enable */
#define	PMCTL_LINKLOSS	0x00000001	/* link loss monitor enable */

#define	SIP_PMEVT	0xb4	/* power management wake-up evnt reg */
#define	PMEVT_ALLFRMMAT	0x40000000	/* receive packet ok */
#define	PMEVT_FRM3MAT	0x04000000	/* match 3rd wake-up frame */
#define	PMEVT_FRM2MAT	0x02000000	/* match 2nd wake-up frame */
#define	PMEVT_FRM1MAT	0x01000000	/* match 1st wake-up frame */
#define	PMEVT_MAGICPKT	0x00000400	/* Magic Packet */
#define	PMEVT_ONEVT	0x00000002	/* link on event */
#define	PMEVT_LOSSEVT	0x00000001	/* link loss event */

#define	SIP_WAKECRC	0xbc	/* wake-up frame CRC register */

#define	SIP_WAKEMASK0	0xc0	/* wake-up frame mask registers */
#define	SIP_WAKEMASK1	0xc4
#define	SIP_WAKEMASK2	0xc8
#define	SIP_WAKEMASK3	0xcc
#define	SIP_WAKEMASK4	0xe0
#define	SIP_WAKEMASK5	0xe4
#define	SIP_WAKEMASK6	0xe8
#define	SIP_WAKEMASK7	0xec
/* #endif DP83820 */

/*
 * Revision codes for the SiS 630 chipset built-in Ethernet.
 */
#define	SIS_REV_900B	0x03
#define	SIS_REV_630E	0x81
#define	SIS_REV_630S	0x82
#define	SIS_REV_630EA1	0x83
#define	SIS_REV_630ET	0x84
#define	SIS_REV_635	0x90	/* same for 735 (745?) */
#define	SIS_REV_960	0x91

/*
 * MII operations for recent SiS chipsets
 */
#define	SIS_MII_STARTDELIM	0x01
#define	SIS_MII_READOP		0x02
#define	SIS_MII_WRITEOP		0x01
#define	SIS_MII_TURNAROUND	0x02

/*
 * Serial EEPROM opcodes, including the start bit.
 */
#define	SIP_EEPROM_OPC_ERASE	0x04
#define	SIP_EEPROM_OPC_WRITE	0x05
#define	SIP_EEPROM_OPC_READ	0x06

/*
 * Serial EEPROM address map (byte address) for the SiS900.
 */
#define	SIP_EEPROM_SIGNATURE	0x00	/* SiS 900 signature */
#define	SIP_EEPROM_MASK		0x02	/* `enable' mask */
#define	SIP_EEPROM_VENDOR_ID	0x04	/* PCI vendor ID */
#define	SIP_EEPROM_DEVICE_ID	0x06	/* PCI device ID */
#define	SIP_EEPROM_SUBVENDOR_ID	0x08	/* PCI subvendor ID */
#define	SIP_EEPROM_SUBSYSTEM_ID	0x0a	/* PCI subsystem ID */
#define	SIP_EEPROM_PMC		0x0c	/* PCI power management capabilities */
#define	SIP_EEPROM_reserved	0x0e	/* reserved */
#define	SIP_EEPROM_ETHERNET_ID0	0x10	/* Ethernet address 0, 1 */
#define	SIP_EEPROM_ETHERNET_ID1	0x12	/* Ethernet address 2, 3 */
#define	SIP_EEPROM_ETHERNET_ID2	0x14	/* Ethernet address 4, 5 */
#define	SIP_EEPROM_CHECKSUM	0x16	/* checksum */

/*
 * Serial EEPROM data (byte addresses) for the DP83815.
 */
#define	SIP_DP83815_EEPROM_CHECKSUM	0x16	/* checksum */
#define	SIP_DP83815_EEPROM_LENGTH	0x18	/* length of EEPROM data */

/*
 * Serial EEPROM data (byte addresses) for the DP83820.
 */
#define	SIP_DP83820_EEPROM_SUBSYSTEM_ID	0x00	/* PCI subsystem ID */
#define	SIP_DP83820_EEPROM_SUBVENDOR_ID	0x02	/* PCI subvendor ID */
#define	SIP_DP83820_EEPROM_CFGINT	0x04	/* PCI INT [31:16] */
#define	SIP_DP83820_EEPROM_CONFIG0	0x06	/* configuration word 0 */
#define	SIP_DP83820_EEPROM_CONFIG1	0x08	/* configuration word 1 */
#define	SIP_DP83820_EEPROM_CONFIG2	0x0a	/* configuration word 2 */
#define	SIP_DP83820_EEPROM_CONFIG3	0x0c	/* configuration word 3 */
#define	SIP_DP83820_EEPROM_SOPAS0	0x0e	/* SecureOn [47:32] */
#define	SIP_DP83820_EEPROM_SOPAS1	0x10	/* SecureOn [31:16] */
#define	SIP_DP83820_EEPROM_SOPAS2	0x12	/* SecureOn [15:0] */
#define	SIP_DP83820_EEPROM_PMATCH0	0x14	/* MAC [47:32] */
#define	SIP_DP83820_EEPROM_PMATCH1	0x16	/* MAC [31:16] */
#define	SIP_DP83820_EEPROM_PMATCH2	0x18	/* MAC [15:0] */
#define	SIP_DP83820_EEPROM_CHECKSUM	0x1a	/* checksum */
#define	SIP_DP83820_EEPROM_LENGTH	0x1c	/* length of EEPROM data */

#define	DP83820_CONFIG2_CFG_EXT_125	(1U << 0)
#define	DP83820_CONFIG2_CFG_M64ADDR	(1U << 1)
#define	DP83820_CONFIG2_CFG_DATA64_EN	(1U << 2)
#define	DP83820_CONFIG2_CFG_T64ADDR	(1U << 3)
#define	DP83820_CONFIG2_CFG_MWI_DIS	(1U << 4)
#define	DP83820_CONFIG2_CFG_MRM_DIS	(1U << 5)
#define	DP83820_CONFIG2_CFG_MODE_1000	(1U << 7)
#define	DP83820_CONFIG2_CFG_TBI_EN	(1U << 9)

#endif /* _DEV_PCI_IF_SIPREG_H_ */
