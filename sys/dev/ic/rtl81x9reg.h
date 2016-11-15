/*	$NetBSD: rtl81x9reg.h,v 1.47 2015/08/28 13:20:46 nonaka Exp $	*/

/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	FreeBSD Id: if_rlreg.h,v 1.9 1999/06/20 18:56:09 wpaul Exp
 */

/*
 * RealTek 8129/8139 register offsets
 */
#define RTK_IDR0	0x0000		/* ID register 0 (station addr) */
#define RTK_IDR1	0x0001		/* Must use 32-bit accesses (?) */
#define RTK_IDR2	0x0002
#define RTK_IDR3	0x0003
#define RTK_IDR4	0x0004
#define RTK_IDR5	0x0005
					/* 0006-0007 reserved */
#define RTK_MAR0	0x0008		/* Multicast hash table */
#define RTK_MAR1	0x0009
#define RTK_MAR2	0x000A
#define RTK_MAR3	0x000B
#define RTK_MAR4	0x000C
#define RTK_MAR5	0x000D
#define RTK_MAR6	0x000E
#define RTK_MAR7	0x000F

#define RTK_TXSTAT0	0x0010		/* status of TX descriptor 0 */
#define RTK_TXSTAT1	0x0014		/* status of TX descriptor 1 */
#define RTK_TXSTAT2	0x0018		/* status of TX descriptor 2 */
#define RTK_TXSTAT3	0x001C		/* status of TX descriptor 3 */

#define RTK_TXADDR0	0x0020		/* address of TX descriptor 0 */
#define RTK_TXADDR1	0x0024		/* address of TX descriptor 1 */
#define RTK_TXADDR2	0x0028		/* address of TX descriptor 2 */
#define RTK_TXADDR3	0x002C		/* address of TX descriptor 3 */

#define RTK_RXADDR		0x0030	/* RX ring start address */
#define RTK_RX_EARLY_BYTES	0x0034	/* RX early byte count */
#define RTK_RX_EARLY_STAT	0x0036	/* RX early status */
#define RTK_COMMAND	0x0037		/* command register */
#define RTK_CURRXADDR	0x0038		/* current address of packet read */
#define RTK_CURRXBUF	0x003A		/* current RX buffer address */
#define RTK_IMR		0x003C		/* interrupt mask register */
#define RTK_ISR		0x003E		/* interrupt status register */
#define RTK_TXCFG	0x0040		/* transmit config */
#define RTK_RXCFG	0x0044		/* receive config */
#define RTK_TIMERCNT	0x0048		/* timer count register */
#define RTK_MISSEDPKT	0x004C		/* missed packet counter */
#define RTK_EECMD	0x0050		/* EEPROM command register */
#define RTK_CFG0	0x0051		/* config register #0 */
#define RTK_CFG1	0x0052		/* config register #1 */
					/* 0053-0057 reserved */
#define RTK_MEDIASTAT	0x0058		/* media status register (8139) */
					/* 0059-005A reserved */
#define RTK_MII		0x005A		/* 8129 chip only */
#define RTK_HALTCLK	0x005B
#define RTK_MULTIINTR	0x005C		/* multiple interrupt */
#define RTK_PCIREV	0x005E		/* PCI revision value */
					/* 005F reserved */
#define RTK_TXSTAT_ALL	0x0060		/* TX status of all descriptors */

/* Direct PHY access registers only available on 8139 */
#define RTK_BMCR	0x0062		/* PHY basic mode control */
#define RTK_BMSR	0x0064		/* PHY basic mode status */
#define RTK_ANAR	0x0066		/* PHY autoneg advert */
#define RTK_LPAR	0x0068		/* PHY link partner ability */
#define RTK_ANER	0x006A		/* PHY autoneg expansion */

#define RTK_DISCCNT	0x006C		/* disconnect counter */
#define RTK_FALSECAR	0x006E		/* false carrier counter */
#define RTK_NWAYTST	0x0070		/* NWAY test register */
#define RTK_RX_ER	0x0072		/* RX_ER counter */
#define RTK_CSCFG	0x0074		/* CS configuration register */

/*
 * When operating in special C+ mode, some of the registers in an
 * 8139C+ chip have different definitions. These are also used for
 * the 8169 gigE chip.
 */
#define RTK_DUMPSTATS_LO	0x0010	/* counter dump command register */
#define RTK_DUMPSTATS_HI	0x0014	/* counter dump command register */
#define RTK_TXLIST_ADDR_LO	0x0020	/* 64 bits, 256 byte alignment */
#define RTK_TXLIST_ADDR_HI	0x0024	/* 64 bits, 256 byte alignment */
#define RTK_TXLIST_ADDR_HPRIO_LO	0x0028	/* 64 bits, 256 byte aligned */
#define RTK_TXLIST_ADDR_HPRIO_HI	0x002C	/* 64 bits, 256 byte aligned */
#define RTK_CFG2		0x0053
#define RTK_TIMERINT		0x0054	/* interrupt on timer expire */
#define RTK_TXSTART		0x00D9	/* 8 bits */
#define RTK_CPLUS_CMD		0x00E0	/* 16 bits */
#define RTK_RXLIST_ADDR_LO	0x00E4	/* 64 bits, 256 byte alignment */
#define RTK_RXLIST_ADDR_HI	0x00E8	/* 64 bits, 256 byte alignment */
#define RTK_EARLY_TX_THRESH	0x00EC	/* 8 bits */

/*
 * Registers specific to the 8169 gigE chip
 */
#define RTK_GTXSTART		0x0038	/* 8 bits */
#define RTK_TIMERINT_8169	0x0058	/* different offset than 8139 */
#define RTK_PHYAR		0x0060
#define RTK_CSIDR		0x0064
#define RTK_CSIAR		0x0068
#define RTK_TBI_LPAR		0x006A
#define RTK_GMEDIASTAT		0x006C	/* 8 bits */
#define RTK_PMCH		0x006F	/* 8 bits */
#define RTK_EPHYAR		0x0080
#define RTK_LDPS		0x0082	/* Link Down Power Saving */
#define RTK_DBG_REG		0x00D1
#define RTK_MAXRXPKTLEN		0x00DA	/* 16 bits, chip multiplies by 8 */
#define RTK_IM			0x00E2
#define RTK_MISC		0x00F0

/*
 * TX config register bits
 */
#define RTK_TXCFG_CLRABRT	0x00000001	/* retransmit aborted pkt */
#define RTK_TXCFG_MAXDMA	0x00000700	/* max DMA burst size */
#define RTK_TXCFG_CRCAPPEND	0x00010000	/* CRC append (0 = yes) */
#define RTK_TXCFG_LOOPBKTST	0x00060000	/* loopback test */
#define RTK_TXCFG_IFG2		0x00080000	/* 8169 only */
#define RTK_TXCFG_IFG		0x03000000	/* interframe gap */
#define RTK_TXCFG_HWREV		0x7CC00000

#define RTK_LOOPTEST_OFF		0x00000000
#define RTK_LOOPTEST_ON		0x00020000
#define RTK_LOOPTEST_ON_CPLUS	0x00060000

/* Known revision codes. */
#define RTK_HWREV_8169		0x00000000
#define RTK_HWREV_8110S		0x00800000
#define RTK_HWREV_8169S		0x04000000
#define RTK_HWREV_8169_8110SB	0x10000000
#define RTK_HWREV_8169_8110SC	0x18000000
#define RTK_HWREV_8102EL	0x24800000
#define RTK_HWREV_8103E		0x24C00000
#define RTK_HWREV_8168D		0x28000000
#define RTK_HWREV_8168DP	0x28800000
#define RTK_HWREV_8168E		0x2C000000
#define RTK_HWREV_8168E_VL	0x2C800000
#define RTK_HWREV_8168_SPIN1	0x30000000
#define RTK_HWREV_8168G		0x4c000000
#define RTK_HWREV_8168G_SPIN1	0x4c100000
#define RTK_HWREV_8168G_SPIN2	0x50900000
#define RTK_HWREV_8168G_SPIN4	0x5c800000
#define RTK_HWREV_8168GU	0x50800000
#define RTK_HWREV_8100E		0x30800000
#define RTK_HWREV_8101E		0x34000000
#define RTK_HWREV_8102E		0x34800000
#define RTK_HWREV_8168_SPIN2	0x38000000
#define RTK_HWREV_8168_SPIN3	0x38400000
#define RTK_HWREV_8100E_SPIN2	0x38800000
#define RTK_HWREV_8168C		0x3C000000
#define RTK_HWREV_8168C_SPIN2	0x3C400000
#define RTK_HWREV_8168CP	0x3C800000
#define RTK_HWREV_8168F		0x48000000
#define RTK_HWREV_8168H		0x54000000
#define RTK_HWREV_8168H_SPIN1	0x54100000
#define RTK_HWREV_8139		0x60000000
#define RTK_HWREV_8139A		0x70000000
#define RTK_HWREV_8139AG	0x70800000
#define RTK_HWREV_8139B		0x78000000
#define RTK_HWREV_8130		0x7C000000
#define RTK_HWREV_8139C		0x74000000
#define RTK_HWREV_8139D		0x74400000
#define RTK_HWREV_8139CPLUS	0x74800000
#define RTK_HWREV_8101		0x74c00000
#define RTK_HWREV_8100		0x78800000
#define RTK_HWREV_8169_8110SBL	0x7cc00000

#define RTK_TXDMA_16BYTES	0x00000000
#define RTK_TXDMA_32BYTES	0x00000100
#define RTK_TXDMA_64BYTES	0x00000200
#define RTK_TXDMA_128BYTES	0x00000300
#define RTK_TXDMA_256BYTES	0x00000400
#define RTK_TXDMA_512BYTES	0x00000500
#define RTK_TXDMA_1024BYTES	0x00000600
#define RTK_TXDMA_2048BYTES	0x00000700

/*
 * Transmit descriptor status register bits.
 */
#define RTK_TXSTAT_LENMASK	0x00001FFF
#define RTK_TXSTAT_OWN		0x00002000
#define RTK_TXSTAT_TX_UNDERRUN	0x00004000
#define RTK_TXSTAT_TX_OK	0x00008000
#define RTK_TXSTAT_EARLY_THRESH	0x003F0000
#define RTK_TXSTAT_COLLCNT	0x0F000000
#define RTK_TXSTAT_CARR_HBEAT	0x10000000
#define RTK_TXSTAT_OUTOFWIN	0x20000000
#define RTK_TXSTAT_TXABRT	0x40000000
#define RTK_TXSTAT_CARRLOSS	0x80000000

#define RTK_TXSTAT_THRESH(x)	(((x) << 16) & RTK_TXSTAT_EARLY_THRESH)
#define RTK_TXTH_256		8	/* (x) * 32 bytes */
#define RTK_TXTH_1536		48

/* MISC register */
#define	RTK_MISC_TXPLA_RST	__BIT(29)
#define	RTK_MISC_DISABLE_LAN_EN	__BIT(23)	/* Enable GPIO pin */
#define	RTK_MISC_PWM_EN		__BIT(22)
#define	RTK_MISC_RXDV_GATED_EN	__BIT(19)
#define	RTK_MISC_EARLY_TALLY_EN	__BIT(16)


/*
 * Interrupt status register bits.
 */
#define RTK_ISR_RX_OK		0x0001
#define RTK_ISR_RX_ERR		0x0002
#define RTK_ISR_TX_OK		0x0004
#define RTK_ISR_TX_ERR		0x0008
#define RTK_ISR_RX_OVERRUN	0x0010
#define RTK_ISR_PKT_UNDERRUN	0x0020
#define RTK_ISR_LINKCHG		0x0020	/* 8169 only */
#define RTK_ISR_FIFO_OFLOW	0x0040	/* 8139 only */
#define RTK_ISR_TX_DESC_UNAVAIL	0x0080	/* C+ only */
#define RTK_ISR_SWI		0x0100	/* C+ only */
#define RTK_ISR_CABLE_LEN_CHGD	0x2000
#define RTK_ISR_PCS_TIMEOUT	0x4000	/* 8129 only */
#define RTK_ISR_TIMEOUT_EXPIRED	0x4000
#define RTK_ISR_SYSTEM_ERR	0x8000

#define RTK_INTRS	\
	(RTK_ISR_TX_OK|RTK_ISR_RX_OK|RTK_ISR_RX_ERR|RTK_ISR_TX_ERR|	\
	RTK_ISR_RX_OVERRUN|RTK_ISR_PKT_UNDERRUN|RTK_ISR_FIFO_OFLOW|	\
	RTK_ISR_PCS_TIMEOUT|RTK_ISR_SYSTEM_ERR)

#define RTK_INTRS_CPLUS	\
	(RTK_ISR_RX_OK|RTK_ISR_RX_ERR|RTK_ISR_TX_ERR|			\
	RTK_ISR_RX_OVERRUN|RTK_ISR_PKT_UNDERRUN|RTK_ISR_FIFO_OFLOW|	\
	RTK_ISR_PCS_TIMEOUT|RTK_ISR_SYSTEM_ERR|RTK_ISR_TIMEOUT_EXPIRED)


/*
 * Media status register. (8139 only)
 */
#define RTK_MEDIASTAT_RXPAUSE	0x01
#define RTK_MEDIASTAT_TXPAUSE	0x02
#define RTK_MEDIASTAT_LINK	0x04
#define RTK_MEDIASTAT_SPEED10	0x08
#define RTK_MEDIASTAT_RXFLOWCTL	0x40	/* duplex mode */
#define RTK_MEDIASTAT_TXFLOWCTL	0x80	/* duplex mode */

/*
 * Receive config register.
 */
#define RTK_RXCFG_RX_ALLPHYS	0x00000001	/* accept all nodes */
#define RTK_RXCFG_RX_INDIV	0x00000002	/* match filter */
#define RTK_RXCFG_RX_MULTI	0x00000004	/* accept all multicast */
#define RTK_RXCFG_RX_BROAD	0x00000008	/* accept all broadcast */
#define RTK_RXCFG_RX_RUNT	0x00000010
#define RTK_RXCFG_RX_ERRPKT	0x00000020
#define RTK_RXCFG_WRAP		0x00000080
#define RTK_RXCFG_MAXDMA	0x00000700
#define RTK_RXCFG_BUFSZ		0x00001800
#define RTK_RXCFG_FIFOTHRESH	0x0000E000
#define RTK_RXCFG_EARLYTHRESH	0x07000000

#define RTK_RXDMA_16BYTES	0x00000000
#define RTK_RXDMA_32BYTES	0x00000100
#define RTK_RXDMA_64BYTES	0x00000200
#define RTK_RXDMA_128BYTES	0x00000300
#define RTK_RXDMA_256BYTES	0x00000400
#define RTK_RXDMA_512BYTES	0x00000500
#define RTK_RXDMA_1024BYTES	0x00000600
#define RTK_RXDMA_UNLIMITED	0x00000700

#define RTK_RXBUF_8		0x00000000
#define RTK_RXBUF_16		0x00000800
#define RTK_RXBUF_32		0x00001000
#define RTK_RXBUF_64		0x00001800
#define RTK_RXBUF_LEN(x)	(1 << (((x) >> 11) + 13))

#define RTK_RXFIFO_16BYTES	0x00000000
#define RTK_RXFIFO_32BYTES	0x00002000
#define RTK_RXFIFO_64BYTES	0x00004000
#define RTK_RXFIFO_128BYTES	0x00006000
#define RTK_RXFIFO_256BYTES	0x00008000
#define RTK_RXFIFO_512BYTES	0x0000A000
#define RTK_RXFIFO_1024BYTES	0x0000C000
#define RTK_RXFIFO_NOTHRESH	0x0000E000

/*
 * Bits in RX status header (included with RX'ed packet
 * in ring buffer).
 */
#define RTK_RXSTAT_RXOK		0x00000001
#define RTK_RXSTAT_ALIGNERR	0x00000002
#define RTK_RXSTAT_CRCERR	0x00000004
#define RTK_RXSTAT_GIANT	0x00000008
#define RTK_RXSTAT_RUNT		0x00000010
#define RTK_RXSTAT_BADSYM	0x00000020
#define RTK_RXSTAT_BROAD	0x00002000
#define RTK_RXSTAT_INDIV	0x00004000
#define RTK_RXSTAT_MULTI	0x00008000
#define RTK_RXSTAT_LENMASK	0xFFFF0000

#define RTK_RXSTAT_UNFINISHED	0xFFF0		/* DMA still in progress */
/*
 * Command register.
 */
#define RTK_CMD_EMPTY_RXBUF	0x0001
#define RTK_CMD_TX_ENB		0x0004
#define RTK_CMD_RX_ENB		0x0008
#define RTK_CMD_RESET		0x0010
#define RTK_CMD_STOPREQ		0x0080

/*
 * EEPROM control register
 */
#define RTK_EE_DATAOUT		0x01	/* Data out */
#define RTK_EE_DATAIN		0x02	/* Data in */
#define RTK_EE_CLK		0x04	/* clock */
#define RTK_EE_SEL		0x08	/* chip select */
#define RTK_EE_MODE		(0x40|0x80)

#define RTK_EEMODE_OFF		0x00
#define RTK_EEMODE_AUTOLOAD	0x40
#define RTK_EEMODE_PROGRAM	0x80
#define RTK_EEMODE_WRITECFG	(0x80|0x40)

/* 9346/9356 EEPROM commands */
#define RTK_EEADDR_LEN0		6	/* 9346 */
#define RTK_EEADDR_LEN1		8	/* 9356 */
#define RTK_EECMD_LEN		4

#define RTK_EECMD_WRITE		0x5	/* 0101b */
#define RTK_EECMD_READ		0x6	/* 0110b */
#define RTK_EECMD_ERASE		0x7	/* 0111b */

#define RTK_EE_ID		0x00
#define RTK_EE_PCI_VID		0x01
#define RTK_EE_PCI_DID		0x02
/* Location of station address inside EEPROM */
#define RTK_EE_EADDR0		0x07
#define RTK_EE_EADDR1		0x08
#define RTK_EE_EADDR2		0x09

/*
 * MII register (8129 only)
 */
#define RTK_MII_CLK		0x01
#define RTK_MII_DATAIN		0x02
#define RTK_MII_DATAOUT		0x04
#define RTK_MII_DIR		0x80	/* 0 == input, 1 == output */

/*
 * Config 0 register
 */
#define RTK_CFG0_ROM0		0x01
#define RTK_CFG0_ROM1		0x02
#define RTK_CFG0_ROM2		0x04
#define RTK_CFG0_PL0		0x08
#define RTK_CFG0_PL1		0x10
#define RTK_CFG0_10MBPS		0x20	/* 10 Mbps internal mode */
#define RTK_CFG0_PCS		0x40
#define RTK_CFG0_SCR		0x80

/*
 * Config 1 register
 */
#define RTK_CFG1_PWRDWN		0x01
#define RTK_CFG1_SLEEP		0x02
#define RTK_CFG1_IOMAP		0x04
#define RTK_CFG1_MEMMAP		0x08
#define RTK_CFG1_RSVD		0x10
#define RTK_CFG1_DRVLOAD	0x20
#define RTK_CFG1_LED0		0x40
#define RTK_CFG1_FULLDUPLEX	0x40	/* 8129 only */
#define RTK_CFG1_LED1		0x80

/*
 * 8139C+ register definitions
 */

/* RTK_DUMPSTATS_LO register */

#define RTK_DUMPSTATS_START	0x00000008

/* Transmit start register */

#define RTK_TXSTART_SWI		0x01	/* generate TX interrupt */
#define RTK_TXSTART_START	0x40	/* start normal queue transmit */
#define RTK_TXSTART_HPRIO_START	0x80	/* start hi prio queue transmit */

/*
 * Config 2 register, 8139C+/8169/8169S/8110S only
 */
#define RTK_CFG2_BUSFREQ		0x07
#define RTK_CFG2_BUSWIDTH	0x08
#define RTK_CFG2_AUXPWRSTS	0x10

#define RTK_BUSFREQ_33MHZ	0x00
#define RTK_BUSFREQ_66MHZ	0x01

#define RTK_BUSWIDTH_32BITS	0x00
#define RTK_BUSWIDTH_64BITS	0x08

/* C+ mode command register */

#define RE_CPLUSCMD_TXENB	0x0001	/* enable C+ transmit mode */
#define RE_CPLUSCMD_RXENB	0x0002	/* enable C+ receive mode */
#define RE_CPLUSCMD_PCI_MRW	0x0008	/* enable PCI multi-read/write */
#define RE_CPLUSCMD_PCI_DAC	0x0010	/* PCI dual-address cycle only */
#define RE_CPLUSCMD_RXCSUM_ENB	0x0020	/* enable RX checksum offload */
#define RE_CPLUSCMD_VLANSTRIP	0x0040	/* enable VLAN tag stripping */
#define RE_CPLUSCMD_MACSTAT_DIS	0x0080	/* 8168B/C/CP */
#define RE_CPLUSCMD_ASF		0x0100	/* 8168C/CP */
#define RE_CPLUSCMD_DBG_SEL	0x0200	/* 8168C/CP */
#define RE_CPLUSCMD_FORCE_TXFC	0x0400	/* 8168C/CP */
#define RE_CPLUSCMD_FORCE_RXFC	0x0800	/* 8168C/CP */
#define RE_CPLUSCMD_FORCE_HDPX	0x1000	/* 8168C/CP */
#define RE_CPLUSCMD_NORMAL_MODE	0x2000	/* 8168C/CP */
#define RE_CPLUSCMD_DBG_ENB	0x4000	/* 8168C/CP */
#define RE_CPLUSCMD_BIST_ENB	0x8000	/* 8168C/CP */

/* C+ early transmit threshold */

#define RTK_EARLYTXTHRESH_CNT	0x003F	/* byte count times 8 */

/*
 * Gigabit PHY access register (8169 only)
 */

#define RTK_PHYAR_PHYDATA	0x0000FFFF
#define RTK_PHYAR_PHYREG		0x001F0000
#define RTK_PHYAR_BUSY		0x80000000

/*
 * Gigabit media status (8169 only)
 */
#define RTK_GMEDIASTAT_FDX	0x01	/* full duplex */
#define RTK_GMEDIASTAT_LINK	0x02	/* link up */
#define RTK_GMEDIASTAT_10MBPS	0x04	/* 10mps link */
#define RTK_GMEDIASTAT_100MBPS	0x08	/* 100mbps link */
#define RTK_GMEDIASTAT_1000MBPS	0x10	/* gigE link */
#define RTK_GMEDIASTAT_RXFLOW	0x20	/* RX flow control on */
#define RTK_GMEDIASTAT_TXFLOW	0x40	/* TX flow control on */
#define RTK_GMEDIASTAT_TBI	0x80	/* TBI enabled */


#define RTK_TX_EARLYTHRESH	((256 / 32) << 16)
#define RTK_RX_FIFOTHRESH	RTK_RXFIFO_256BYTES
#define RTK_RX_MAXDMA		RTK_RXDMA_256BYTES
#define RTK_TX_MAXDMA		RTK_TXDMA_256BYTES

#define RTK_RXCFG_CONFIG	(RTK_RX_FIFOTHRESH|RTK_RX_MAXDMA|RTK_RX_BUF_SZ)
#define RTK_TXCFG_CONFIG	(RTK_TXCFG_IFG|RTK_TX_MAXDMA)

#define RE_RX_FIFOTHRESH	RTK_RXFIFO_NOTHRESH
#define RE_RX_MAXDMA		RTK_RXDMA_UNLIMITED
#define RE_TX_MAXDMA		RTK_TXDMA_2048BYTES

#define RE_RXCFG_CONFIG		(RE_RX_FIFOTHRESH|RE_RX_MAXDMA|RTK_RX_BUF_SZ)
#define RE_TXCFG_CONFIG		(RTK_TXCFG_IFG|RE_TX_MAXDMA)

/*
 * RX/TX descriptor definition. When large send mode is enabled, the
 * lower 11 bits of the TX rtk_cmd word are used to hold the MSS, and
 * the checksum offload bits are disabled. The structure layout is
 * the same for RX and TX descriptors
 */

struct re_desc {
	volatile uint32_t	re_cmdstat;
	volatile uint32_t	re_vlanctl;
	volatile uint32_t	re_bufaddr_lo;
	volatile uint32_t	re_bufaddr_hi;
};

#define RE_TDESC_CMD_FRAGLEN	0x0000FFFF
#define RE_TDESC_CMD_TCPCSUM	0x00010000	/* TCP checksum enable */
#define RE_TDESC_CMD_UDPCSUM	0x00020000	/* UDP checksum enable */
#define RE_TDESC_CMD_IPCSUM	0x00040000	/* IP header checksum enable */
#define RE_TDESC_CMD_MSSVAL	0x07FF0000	/* Large send MSS value */
#define RE_TDESC_CMD_MSSVAL_SHIFT 16		/* Shift of the above */
#define RE_TDESC_CMD_LGSEND	0x08000000	/* TCP large send enb */
#define RE_TDESC_CMD_EOF	0x10000000	/* end of frame marker */
#define RE_TDESC_CMD_SOF	0x20000000	/* start of frame marker */
#define RE_TDESC_CMD_EOR	0x40000000	/* end of ring marker */
#define RE_TDESC_CMD_OWN	0x80000000	/* chip owns descriptor */

#define RE_TDESC_VLANCTL_TAG	0x00020000	/* Insert VLAN tag */
#define RE_TDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */
#define RE_TDESC_VLANCTL_UDPCSUM 0x80000000	/* DESCV2 UDP cksum enable */
#define RE_TDESC_VLANCTL_TCPCSUM 0x40000000	/* DESCV2 TCP cksum enable */
#define RE_TDESC_VLANCTL_IPCSUM	0x20000000	/* DESCV2 IP hdr cksum enable */

/*
 * Error bits are valid only on the last descriptor of a frame
 * (i.e. RE_TDESC_CMD_EOF == 1)
 */

#define RE_TDESC_STAT_COLCNT	0x000F0000	/* collision count */
#define RE_TDESC_STAT_EXCESSCOL	0x00100000	/* excessive collisions */
#define RE_TDESC_STAT_LINKFAIL	0x00200000	/* link faulure */
#define RE_TDESC_STAT_OWINCOL	0x00400000	/* out-of-window collision */
#define RE_TDESC_STAT_TXERRSUM	0x00800000	/* transmit error summary */
#define RE_TDESC_STAT_UNDERRUN	0x02000000	/* TX underrun occurred */
#define RE_TDESC_STAT_OWN	0x80000000

/*
 * RX descriptor cmd/vlan definitions
 */

#define RE_RDESC_CMD_EOR	0x40000000
#define RE_RDESC_CMD_OWN	0x80000000
#define RE_RDESC_CMD_BUFLEN	0x00001FFF

#define RE_RDESC_STAT_OWN	0x80000000
#define RE_RDESC_STAT_EOR	0x40000000
#define RE_RDESC_STAT_SOF	0x20000000
#define RE_RDESC_STAT_EOF	0x10000000
#define RE_RDESC_STAT_FRALIGN	0x08000000	/* frame alignment error */
#define RE_RDESC_STAT_MCAST	0x04000000	/* multicast pkt received */
#define RE_RDESC_STAT_UCAST	0x02000000	/* unicast pkt received */
#define RE_RDESC_STAT_BCAST	0x01000000	/* broadcast pkt received */
#define RE_RDESC_STAT_BUFOFLOW	0x00800000	/* out of buffer space */
#define RE_RDESC_STAT_FIFOOFLOW	0x00400000	/* FIFO overrun */
#define RE_RDESC_STAT_GIANT	0x00200000	/* pkt > 4096 bytes */
#define RE_RDESC_STAT_RXERRSUM	0x00100000	/* RX error summary */
#define RE_RDESC_STAT_RUNT	0x00080000	/* runt packet received */
#define RE_RDESC_STAT_CRCERR	0x00040000	/* CRC error */
#define RE_RDESC_STAT_PROTOID	0x00030000	/* Protocol type */
#define RE_RDESC_STAT_IPSUMBAD	0x00008000	/* IP header checksum bad */
#define RE_RDESC_STAT_UDPSUMBAD	0x00004000	/* UDP checksum bad */
#define RE_RDESC_STAT_TCPSUMBAD	0x00002000	/* TCP checksum bad */
#define RE_RDESC_STAT_FRAGLEN	0x00001FFF	/* RX'ed frame/frag len */
#define RE_RDESC_STAT_GFRAGLEN	0x00003FFF	/* RX'ed frame/frag len */

#define RE_RDESC_VLANCTL_TAG	0x00010000	/* VLAN tag available
						   (re_vlandata valid)*/
#define RE_RDESC_VLANCTL_DATA	0x0000FFFF	/* TAG data */
#define RE_RDESC_VLANCTL_IPV6	0x80000000	/* DESCV2 IPV6 packet */
#define RE_RDESC_VLANCTL_IPV4	0x40000000	/* DESCV2 IPV4 packet */

#define RE_PROTOID_NONIP	0x00000000
#define RE_PROTOID_TCPIP	0x00010000
#define RE_PROTOID_UDPIP	0x00020000
#define RE_PROTOID_IP		0x00030000
#define RE_TCPPKT(x)		(((x) & RE_RDESC_STAT_PROTOID) == \
				 RE_PROTOID_TCPIP)
#define RE_UDPPKT(x)		(((x) & RE_RDESC_STAT_PROTOID) == \
				 RE_PROTOID_UDPIP)

#define RE_ADDR_LO(y)		((uint64_t)(y) & 0xFFFFFFFF)
#define RE_ADDR_HI(y)		((uint64_t)(y) >> 32)

/*
 * Statistics counter structure (8139C+ and 8169 only)
 */
struct re_stats {
	uint32_t		re_tx_pkts_lo;
	uint32_t		re_tx_pkts_hi;
	uint32_t		re_tx_errs_lo;
	uint32_t		re_tx_errs_hi;
	uint32_t		re_tx_errs;
	uint16_t		re_missed_pkts;
	uint16_t		re_rx_framealign_errs;
	uint32_t		re_tx_onecoll;
	uint32_t		re_tx_multicolls;
	uint32_t		re_rx_ucasts_hi;
	uint32_t		re_rx_ucasts_lo;
	uint32_t		re_rx_bcasts_lo;
	uint32_t		re_rx_bcasts_hi;
	uint32_t		re_rx_mcasts;
	uint16_t		re_tx_aborts;
	uint16_t		re_rx_underruns;
};

#define RE_IFQ_MAXLEN		512

#define RE_JUMBO_FRAMELEN	ETHER_MAX_LEN_JUMBO
#define RE_JUMBO_MTU		ETHERMTU_JUMBO
