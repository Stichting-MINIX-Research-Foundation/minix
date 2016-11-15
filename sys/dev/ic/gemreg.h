/*	$NetBSD: gemreg.h,v 1.15 2012/07/02 11:23:40 jdc Exp $ */

/*
 *
 * Copyright (C) 2001 Eduardo Horvath.
 * All rights reserved.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_IF_GEMREG_H
#define	_IF_GEMREG_H

/*
 * Register definitions for Sun GEM Gigabit Ethernet
 * See `GEM Gigabit Ethernet ASIC Specification'
 *   http://www.sun.com/processors/manuals/ge.pdf
 * and `Sbus GEM Specification'
 *  http://mediacast.sun.com/users/Barton808/media/gem_sbus-1.pdf
 * section 3.1.3 GEM Register Space
 */

/*
 * Global Resources
 * Section 3.1.4.1
 *
 * First bank: this registers live at the start of the PCI
 * mapping, and at the start of the second bank of the SBus
 * version.
 */
#define	GEM_SEB_STATE		0x0000	/* SEB State (R/O) */
#define	GEM_CONFIG		0x0004	/* Configuration */
#define	GEM_STATUS		0x000c	/* Status */
/* Note: Reading the status register auto-clears bits 0-6 */
#define	GEM_INTMASK		0x0010	/* Interrupt Mask */
#define	GEM_INTACK		0x0014	/* Interrupt Acknowledge (W/O) */
#define	GEM_STATUS_ALIAS	0x001c	/* Status Alias */
/* This is the same as GEM_STATUS but reading it does not auto-clear bits. */

/*
 * Second bank: this registers live at offset 0x1000 of the PCI
 * mapping, and at the start of the first bank of the SBus
 * version.
 */
#define GEM_PCI_BANK2_OFFSET	0x1000
#define GEM_PCI_BANK2_SIZE	0x14
#define	GEM_ERROR_STATUS	0x0000	/* PCI Error Status */
#define	GEM_ERROR_MASK		0x0004	/* PCI Error Mask */
#define	GEM_BIF_CONFIG		0x0008	/* PCI BIF Configuration */
#define	GEM_BIF_DIAG		0x000c	/* PCI BIF Diagnostic */
#define	GEM_RESET		0x0010	/* PCI Software Reset */

#define GEM_SBUS_RESET		0x0000	/* SBus Reset */
#define GEM_SBUS_CONFIG		0x0004	/* SBus Burst-Size Configuration */
#define GEM_SBUS_ERROR_STATUS	0x0008	/* SBus Fatal Error */
#define GEM_SBUS_REVISION	0x000c	/* SBus Revision */
/*  SBus Software Reset at same offset (0x0010) as PCI Software Reset above */

/*
 * Bits in GEM_SEB_STATE register
 * For diagnostic use
 */
#define	GEM_SEB_ARB		0x000000002	/* Arbitration status */
#define	GEM_SEB_RXWON		0x000000004

/*
 * Bits in GEM_CONFIG register
 * Default: 0x00042
 */
#define	GEM_CONFIG_BURST_64	0x000000000	/* 0->infinity, 1->64KB */
#define	GEM_CONFIG_BURST_INF	0x000000001	/* 0->infinity, 1->64KB */
#define	GEM_CONFIG_TXDMA_LIMIT	0x00000003e
#define	GEM_CONFIG_RXDMA_LIMIT	0x0000007c0
/* GEM_CONFIG_RONPAULBIT and GEM_CONFIG_BUG2FIX are Apple only. */
#define	GEM_CONFIG_RONPAULBIT	0x000000800	/* after infinite burst use
						 * memory read multiple for
						 * PCI commands */
#define	GEM_CONFIG_BUG2FIX	0x000001000	/* fix RX hang after overflow */

#define	GEM_CONFIG_TXDMA_LIMIT_SHIFT	1
#define	GEM_CONFIG_RXDMA_LIMIT_SHIFT	6


/*
 * Interrupt bits, for both the GEM_STATUS and GEM_INTMASK regs.
 * Bits 0-6 auto-clear when read.
 */
#define	GEM_INTR_TX_INTME	0x000000001	/* Frame w/INTME bit set sent */
#define	GEM_INTR_TX_EMPTY	0x000000002	/* TX ring empty */
#define	GEM_INTR_TX_DONE	0x000000004	/* TX complete */
#define	GEM_INTR_RX_DONE	0x000000010	/* Got a packet */
#define	GEM_INTR_RX_NOBUF	0x000000020	/* No free receive buffers */
#define	GEM_INTR_RX_TAG_ERR	0x000000040	/* RX Tag framing error */
#define	GEM_INTR_PERR		0x000000080	/* Parity error */
#define	GEM_INTR_PCS		0x000002000	/* PCS interrupt */
#define	GEM_INTR_TX_MAC		0x000004000	/* TX MAC interrupt */
#define	GEM_INTR_RX_MAC		0x000008000	/* RX MAC interrupt */
#define	GEM_INTR_MAC_CONTROL	0x000010000	/* MAC control interrupt */
#define	GEM_INTR_MIF		0x000020000	/* MIF interrupt */
#define	GEM_INTR_BERR		0x000040000	/* Bus error interrupt */
#define GEM_INTR_BITS	"\177\020"					\
			"b\0INTME\0b\1TXEMPTY\0b\2TXDONE\0"		\
			"b\4RXDONE\0b\5RXNOBUF\0b\6RX_TAG_ERR\0"	\
			"b\xdPCS\0b\xeTXMAC\0b\xfRXMAC\0"		\
			"b\x10MAC_CONTROL\0b\x11MIF\0b\x12IBERR\0\0"

/* Top part (bits 19-31) of GEM_STATUS has TX completion information */
#define	GEM_STATUS_TX_COMPL	0xfff800000	/* TX completion reg. */


/*
 * Bits in GEM_ERROR_STATUS and GEM_ERROR_MASK PCI registers
 */
#define	GEM_ERROR_STAT_BADACK	0x000000001	/* No ACK64# */
#define	GEM_ERROR_STAT_DTRTO	0x000000002	/* Delayed xaction timeout */
#define	GEM_ERROR_STAT_OTHERS	0x000000004	/* Other PCI errors.  Read PCI
						   Status Register in PCI
						   Configuration space */
#define	GEM_ERROR_BITS		"\177\020b\0ACKBAD\0b\1DTRTO\0b\2OTHER\0\0"


/*
 * Bits in GEM_SBUS_CONFIG register
 */
#define GEM_SBUS_CFG_BSIZE32	0x00000001
#define GEM_SBUS_CFG_BSIZE64	0x00000002
#define GEM_SBUS_CFG_BSIZE128	0x00000004
#define GEM_SBUS_CFG_BMODE64	0x00000008
#define GEM_SBUS_CFG_PARITY	0x00000200


/*
 * Bits in GEM_BIF_CONFIG register
 * Default: 0x0
 */
#define	GEM_BIF_CONFIG_SLOWCLK	0x000000001	/* Parity error timing */
#define	GEM_BIF_CONFIG_HOST_64	0x000000002	/* 64-bit host */
#define	GEM_BIF_CONFIG_B64D_DIS	0x000000004	/* no 64-bit data cycle */
#define	GEM_BIF_CONFIG_M66EN	0x000000008
#define	GEM_BIF_CONFIG_BITS	"\177\020b\0SLOWCLK\0b\1HOST64\0"	\
				"b\2B64DIS\0b\3M66EN\0\0"


/*
 * Bits in GEM_BIF_DIAG register
 * Default: 0x00000000
 */
#define GEN_BIF_DIAG_PCIBURST	0x007f0000	/* PCI Burst Controller state
						 * machine */
#define GEN_BIF_DIAG_STATE	0xff000000	/* BIF state machine */

/*
 * Bits in GEM_RESET register
 * RESET_TX and RESET_RX self clear when complete.
 */
#define	GEM_RESET_TX		0x000000001	/* Reset TX half */
#define	GEM_RESET_RX		0x000000002	/* Reset RX half */
#define	GEM_RESET_GLOBAL	0x000000003	/* Global Reset */
#define	GEM_RESET_RSTOUT	0x000000004	/* Force PCI RSTOUT# */


/*
 * TX DMA Programmable Resources
 * Section 3.1.4.2
 * The 53 most significant bits of the Descriptor Base Low/High registers
 * are used as the TX descriptor ring base address.  The ring base must be
 * initialized to a 2KByte-aligned address after power-on or software reset.
 */
#define	GEM_TX_KICK		0x2000		/* TX Kick */
/* Note: Write last valid desc + 1 */
#define	GEM_TX_CONFIG		0x2004		/* TX Configuration */
#define	GEM_TX_RING_PTR_LO	0x2008		/* TX Descriptor Base Low */
#define	GEM_TX_RING_PTR_HI	0x200c		/* TX Descriptor Base High */
/*				0x2010		   Reserved */
#define	GEM_TX_FIFO_WR_PTR	0x2014		/* TX FIFO Write Pointer */
#define	GEM_TX_FIFO_SDWR_PTR	0x2018		/* TX FIFO Shadow Write Ptr */
#define	GEM_TX_FIFO_RD_PTR	0x201c		/* TX FIFO Read Pointer */
#define	GEM_TX_FIFO_SDRD_PTR	0x2020		/* TX FIFO Shadow Read Ptr */
#define	GEM_TX_FIFO_PKT_CNT	0x2024		/* TX FIFO Packet Counter */
#define	GEM_TX_STATE_MACHINE	0x2028		/* TX State Machine */
/*				0x202c		   Unknown */
#define	GEM_TX_DATA_PTR_LO	0x2030		/* TX Data Pointer Low */
#define	GEM_TX_DATA_PTR_HI	0x2034		/* TX Data Pointer High */

#define	GEM_TX_COMPLETION	0x2100		/* TX Completion */
#define	GEM_TX_FIFO_ADDRESS	0x2104		/* TX FIFO Address */
#define	GEM_TX_FIFO_TAG		0x2108		/* TX FIFO Tag */
#define	GEM_TX_FIFO_DATA_LO	0x210c		/* TX FIFO Data Low */
#define	GEM_TX_FIFO_DATA_HI_T1	0x2110		/* TX FIFO Data HighT1 */
#define	GEM_TX_FIFO_DATA_HI_T0	0x2114		/* TX FIFO Data HighT0 */
#define	GEM_TX_FIFO_SIZE	0x2118		/* TX FIFO Size */
#define	GEM_TX_DEBUG		0x3028


/*
 * Bits in GEM_TX_CONFIG register
 * Default: 0x118c10
 * TX FIFO Threshold should be set to 0x4ff
 */
#define	GEM_TX_CONFIG_TXDMA_EN	0x00000001	/* TX DMA enable */
#define	GEM_TX_CONFIG_TXRING_SZ	0x0000001e	/* TX ring size */
#define GEM_TX_CONFIG_TXFIFO_SL 0x00000020	/* TX DMA FIFO PIO select */
#define	GEM_TX_CONFIG_TXFIFO_TH	0x001ffc00	/* TX fifo threshold */
#define	GEM_TX_CONFIG_PACED	0x00200000	/* TX_all_int modifier */

#define	GEM_RING_SZ_32		(0<<1)	/* 32 descriptors */
#define	GEM_RING_SZ_64		(1<<1)
#define	GEM_RING_SZ_128		(2<<1)
#define	GEM_RING_SZ_256		(3<<1)
#define	GEM_RING_SZ_512		(4<<1)
#define	GEM_RING_SZ_1024	(5<<1)
#define	GEM_RING_SZ_2048	(6<<1)
#define	GEM_RING_SZ_4096	(7<<1)
#define	GEM_RING_SZ_8192	(8<<1)	/* Default */


/*
 * Bits in GEM_TX_COMPLETION register
 */
#define	GEM_TX_COMPLETION_MASK	0x00001fff	/* # of last descriptor */


/*
 * RX DMA Programmable Resources
 * Section 3.1.4.3
 * The 53 most significant bits of the Descriptor Base Low/High registers
 * are used as the RX descriptor ring base address.  The ring base must be
 * initialized to a 2KByte-aligned address after power-on or software reset.
 */
#define	GEM_RX_CONFIG		0x4000		/* RX Configuration */
#define	GEM_RX_RING_PTR_LO	0x4004		/* RX Descriptor Base Low */
#define	GEM_RX_RING_PTR_HI	0x4008		/* RX Descriptor Base High */
#define	GEM_RX_FIFO_WR_PTR	0x400c		/* RX FIFO Write Pointer */
#define	GEM_RX_FIFO_SDWR_PTR	0x4010		/* RX FIFO Shadow Write Ptr */
#define	GEM_RX_FIFO_RD_PTR	0x4014		/* RX FIFO Read Pointer */
#define	GEM_RX_FIFO_PKT_CNT	0x4018		/* RX FIFO Packet Counter */
#define	GEM_RX_STATE_MACHINE	0x401c		/* RX State Machine */
#define	GEM_RX_PAUSE_THRESH	0x4020		/* Pause Thresholds */
#define	GEM_RX_DATA_PTR_LO	0x4024		/* RX Data Pointer Low */
#define	GEM_RX_DATA_PTR_HI	0x4028		/* RX Data Pointer High */

#define	GEM_RX_KICK		0x4100		/* RX Kick */
/* Note: Write last valid desc + 1.  Must be a multiple of 4 */
#define	GEM_RX_COMPLETION	0x4104		/* RX Completion */
#define	GEM_RX_BLANKING		0x4108		/* RX Blanking */
#define	GEM_RX_FIFO_ADDRESS	0x410c		/* RX FIFO Address */
#define	GEM_RX_FIFO_TAG		0x4110		/* RX FIFO Tag */
#define	GEM_RX_FIFO_DATA_LO	0x4114		/* RX FIFO Data Low */
#define	GEM_RX_FIFO_DATA_HI_T1	0x4118		/* RX FIFO Data HighT0 */
#define	GEM_RX_FIFO_DATA_HI_T0	0x411c		/* RX FIFO Data HighT1 */
#define	GEM_RX_FIFO_SIZE	0x4120		/* RX FIFO Size */


/*
 * Bits in GEM_RX_CONFIG register
 * Default: 0x1000010
 */
#define	GEM_RX_CONFIG_RXDMA_EN	0x00000001	/* RX DMA enable */
#define	GEM_RX_CONFIG_RXRING_SZ	0x0000001e	/* RX ring size */
#define	GEM_RX_CONFIG_BATCH_DIS	0x00000020	/* desc batching disable */
#define	GEM_RX_CONFIG_FBOFF	0x00001c00	/* first byte offset */
#define	GEM_RX_CONFIG_CXM_START	0x000fe000	/* cksum start offset bytes */
#define	GEM_RX_CONFIG_FIFO_THRS	0x07000000	/* fifo threshold size */

#define	GEM_THRSH_64	0
#define	GEM_THRSH_128	1
#define	GEM_THRSH_256	2
#define	GEM_THRSH_512	3
#define	GEM_THRSH_1024	4
#define	GEM_THRSH_2048	5

#define	GEM_RX_CONFIG_FIFO_THRS_SHIFT	24
#define	GEM_RX_CONFIG_FBOFF_SHFT	10
#define	GEM_RX_CONFIG_CXM_START_SHFT	13


/* GEM_RX_PAUSE_THRESH register bits -- sizes in multiples of 64 bytes */
#define	GEM_RX_PTH_XOFF_THRESH	0x000001ff
#define	GEM_RX_PTH_XON_THRESH	0x001ff000


/* GEM_RX_BLANKING register bits */
#define	GEM_RX_BLANKING_PACKETS	0x000001ff	/* Delay intr for x packets */
#define	GEM_RX_BLANKING_TIME	0x000ff000	/* Delay intr for x ticks */
#define	GEM_RX_BLANKING_TIME_SHIFT 12
/* One tick is 2048 PCI clocks, or 16us at 66MHz */


/*
 * MAC Programmable Resources
 * Section 3.1.5
 */
#define	GEM_MAC_TXRESET		0x6000		/* TX MAC Software Reset Cmd */
#define	GEM_MAC_RXRESET		0x6004		/* RX MAC Software Reset Cmd */
/* Note: Store 1, cleared when done for TXRESET and RXRESET */
#define	GEM_MAC_SEND_PAUSE_CMD	0x6008		/* Send Pause Command */
#define	GEM_MAC_TX_STATUS	0x6010		/* TX MAC Status */
#define	GEM_MAC_RX_STATUS	0x6014		/* RX MAC Status */
#define	GEM_MAC_CONTROL_STATUS	0x6018		/* MAC Control Status */
#define	GEM_MAC_TX_MASK		0x6020		/* TX MAC Mask */
#define	GEM_MAC_RX_MASK		0x6024		/* RX MAC Mask */
#define	GEM_MAC_CONTROL_MASK	0x6028		/* MAC Control Mask */
#define	GEM_MAC_TX_CONFIG	0x6030		/* TX MAC Configuration */
#define	GEM_MAC_RX_CONFIG	0x6034		/* XX MAC Configuration */
#define	GEM_MAC_CONTROL_CONFIG	0x6038		/* MAC Control Configuration */
#define	GEM_MAC_XIF_CONFIG	0x603c		/* XIF Configuration */
#define	GEM_MAC_IPG0		0x6040		/* InterPacketGap0 */
#define	GEM_MAC_IPG1		0x6044		/* InterPacketGap1 */
#define	GEM_MAC_IPG2		0x6048		/* InterPacketGap2 */
#define	GEM_MAC_SLOT_TIME	0x604c		/* SlotTime, bits 0-7 */
#define	GEM_MAC_MAC_MIN_FRAME	0x6050		/* MinFrameSize */
#define	GEM_MAC_MAC_MAX_FRAME	0x6054		/* MaxFrameSize */
#define	GEM_MAC_PREAMBLE_LEN	0x6058		/* PA Size */
#define	GEM_MAC_JAM_SIZE	0x605c		/* JamSize */
#define	GEM_MAC_ATTEMPT_LIMIT	0x6060		/* Attempt Limit */
#define	GEM_MAC_CONTROL_TYPE	0x6064		/* MAC Control Type */

#define	GEM_MAC_ADDR0		0x6080		/* Normal MAC address 0 */
#define	GEM_MAC_ADDR1		0x6084
#define	GEM_MAC_ADDR2		0x6088
#define	GEM_MAC_ADDR3		0x608c		/* Alternate MAC address 0 */
#define	GEM_MAC_ADDR4		0x6090
#define	GEM_MAC_ADDR5		0x6094
#define	GEM_MAC_ADDR6		0x6098		/* Control MAC address 0 */
#define	GEM_MAC_ADDR7		0x609c
#define	GEM_MAC_ADDR8		0x60a0

#define	GEM_MAC_ADDR_FILTER0	0x60a4		/* Address Filter */
#define	GEM_MAC_ADDR_FILTER1	0x60a8
#define	GEM_MAC_ADDR_FILTER2	0x60ac
#define	GEM_MAC_ADR_FLT_MASK1_2	0x60b0		/* Address Filter Mask 2&1 */
#define	GEM_MAC_ADR_FLT_MASK0	0x60b4		/* Address Filter Mask 0 */

#define	GEM_MAC_HASH0		0x60c0		/* Hash table 0 */
#define	GEM_MAC_HASH1		0x60c4
#define	GEM_MAC_HASH2		0x60c8
#define	GEM_MAC_HASH3		0x60cc
#define	GEM_MAC_HASH4		0x60d0
#define	GEM_MAC_HASH5		0x60d4
#define	GEM_MAC_HASH6		0x60d8
#define	GEM_MAC_HASH7		0x60dc
#define	GEM_MAC_HASH8		0x60e0
#define	GEM_MAC_HASH9		0x60e4
#define	GEM_MAC_HASH10		0x60e8
#define	GEM_MAC_HASH11		0x60ec
#define	GEM_MAC_HASH12		0x60f0
#define	GEM_MAC_HASH13		0x60f4
#define	GEM_MAC_HASH14		0x60f8
#define	GEM_MAC_HASH15		0x60fc

#define	GEM_MAC_NORM_COLL_CNT	0x6100		/* Normal Collision Counter */
#define	GEM_MAC_FIRST_COLL_CNT	0x6104		/* First Attempt Successful
						   Collision Counter */
#define	GEM_MAC_EXCESS_COLL_CNT	0x6108		/* Excess Collision Counter */
#define	GEM_MAC_LATE_COLL_CNT	0x610c		/* Late Collision Counter */
#define	GEM_MAC_DEFER_TMR_CNT	0x6110		/* Defer Timer */
#define	GEM_MAC_PEAK_ATTEMPTS	0x6114		/* Peak Attempts */
#define	GEM_MAC_RX_FRAME_COUNT	0x6118		/* Receive Frame Counter */
#define	GEM_MAC_RX_LEN_ERR_CNT	0x611c		/* Length Error Counter */
#define	GEM_MAC_RX_ALIGN_ERR	0x6120		/* Alignment Error Counter */
#define	GEM_MAC_RX_CRC_ERR_CNT	0x6124		/* FCS Error Counter */
#define	GEM_MAC_RX_CODE_VIOL	0x6128		/* RX Code Violation Error
						   Counter */

#define	GEM_MAC_RANDOM_SEED	0x6130		/* Random Number Seed */
#define	GEM_MAC_MAC_STATE	0x6134		/* State Machine */


/*
 * Bits in GEM_MAC_SEND_PAUSE_CMD register
 * Pause time is in units of Slot Times.
 */
#define	GEM_MAC_PAUSE_CMD_TIME	0x0000ffff
#define	GEM_MAC_PAUSE_CMD_SEND	0x00010000


/*
 * Bits in GEM_MAC_TX_STATUS and _MASK register
 * Interrupt bits are auto-cleared when the status register is read and
 * the corresponding bit is set in the mask register.
 */
#define	GEM_MAC_TX_XMIT_DONE	0x00000001	/* Successful transmission */
#define	GEM_MAC_TX_UNDERRUN	0x00000002	/* TX "data starvation" */
#define	GEM_MAC_TX_PKT_TOO_LONG	0x00000004	/* Frame exceeds max. length */
#define	GEM_MAC_TX_NCC_EXP	0x00000008	/* Normal collision counter has
						   rolled over */
#define	GEM_MAC_TX_ECC_EXP	0x00000010	/* Excessive coll cnt rolled */
#define	GEM_MAC_TX_LCC_EXP	0x00000020	/* Late coll cnt rolled */
#define	GEM_MAC_TX_FCC_EXP	0x00000040	/* First coll cnt rolled */
#define	GEM_MAC_TX_DEFER_EXP	0x00000080	/* Defer timer cnt rolled */
#define	GEM_MAC_TX_PEAK_EXP	0x00000100	/* Peak attempts cnt rolled */


/*
 * Bits in GEM_MAC_RX_STATUS and _MASK register
 */
#define	GEM_MAC_RX_DONE		0x00000001	/* Successful reception */
#define	GEM_MAC_RX_OVERFLOW	0x00000002	/* RX resource lack */
#define	GEM_MAC_RX_FRAME_CNT	0x00000004	/* Receive frame counter has
						   rolled over */
#define	GEM_MAC_RX_ALIGN_EXP	0x00000008	/* Alignment error cnt rolled */
#define	GEM_MAC_RX_CRC_EXP	0x00000010	/* CRC error cnt rolled */
#define	GEM_MAC_RX_LEN_EXP	0x00000020	/* Length error cnt rolled */
#define	GEM_MAC_RX_CVI_EXP	0x00000040	/* Code violation err rolled */


/*
 * Bits in GEM_MAC_CONTROL_STATUS and GEM_MAC_CONTROL_MASK register
 */
#define	GEM_MAC_PAUSED		0x00000001	/* Pause received */
#define	GEM_MAC_PAUSE		0x00000002	/* enter pause state */
#define	GEM_MAC_RESUME		0x00000004	/* exit pause state */
#define	GEM_MAC_PAUSE_TIME	0xffff0000	/* Pause time received */
#define	GEM_MAC_STATUS_BITS	"\177\020b\0PAUSED\0b\1PAUSE\0b\2RESUME\0\0"


/*
 * Bits in GEM_MAC_XIF_CONFIG register
 * Default: 0x00
 */
#define	GEM_MAC_XIF_TX_MII_ENA	0x00000001	/* Enable MII output */
#define	GEM_MAC_XIF_MII_LOOPBK	0x00000002	/* Enable (G)MII loopback */
#define	GEM_MAC_XIF_ECHO_DISABL	0x00000004	/* Disable echo */
#define	GEM_MAC_XIF_GMII_MODE	0x00000008	/* Select GMII/MII mode */
#define	GEM_MAC_XIF_MII_BUF_ENA	0x00000010	/* Enable MII recv buffers */
#define	GEM_MAC_XIF_LINK_LED	0x00000020	/* force link LED active */
#define	GEM_MAC_XIF_FDPLX_LED	0x00000040	/* force FDPLX LED active */
#define	GEM_MAC_XIF_BITS	"\177\020b\0TXMIIENA\0b\1MIILOOP\0b\2NOECHO" \
				"\0b\3GMII\0b\4MIIBUFENA\0b\5LINKLED\0" \
				"b\6FDLED\0\0"


/*
 * Bits in GEM_MAC_TX_CONFIG register
 * GEM_MAC_TX_ENABLE must be cleared and a delay imposed before writing to
 * other bits in this register or any of the MAC parameters registers.
 * The GEM_MAC_TX_ENABLE bit will read 0 when the transmitter has stopped.
 * Carrier Extension must be set when operating in Half-Duplex at 1Gbps,
 * and disabled otherwise.  To enable this GEM_MAC_TX_CARR_EXTEND and
 * GEM_MAC_RX_CARR_EXTEND must be set to 1 and the Slot Time register must
 * be set to 0x200.
 */
#define	GEM_MAC_TX_ENABLE	0x00000001	/* TX enable */
#define	GEM_MAC_TX_IGN_CARRIER	0x00000002	/* Ignore carrier sense */
#define	GEM_MAC_TX_IGN_COLLIS	0x00000004	/* ignore collisions */
#define	GEM_MAC_TX_ENA_IPG0	0x00000008	/* extend Rx-to-TX IPG */
#define	GEM_MAC_TX_NGU		0x00000010	/* Never give up */
#define	GEM_MAC_TX_NGU_LIMIT	0x00000020	/* Never give up limit */
#define	GEM_MAC_TX_NO_BACKOFF	0x00000040	/* Never backoff on coll */
#define	GEM_MAC_TX_SLOWDOWN	0x00000080	/* Watch carrier sense */
#define	GEM_MAC_TX_NO_FCS	0x00000100	/* no FCS will be generated */
#define	GEM_MAC_TX_CARR_EXTEND	0x00000200	/* Ena TX Carrier Extension */
#define	GEM_MAC_TX_CONFIG_BITS	"\177\020" \
				"b\0TXENA\0b\1IGNCAR\0b\2IGNCOLLIS\0" \
				"b\3IPG0ENA\0b\4TXNGU\0b\5TXNGULIM\0" \
				"b\6NOBKOFF\0b\7SLOWDN\0b\x8NOFCS\0" \
				"b\x9TXCARREXT\0\0"


/*
 * Bits in GEM_MAC_RX_CONFIG register
 * The GEM_MAC_RX_ENABLE bit must be cleared and a delay of 3.2ms imposed
 * before writing to other bits in this register or any of the MAC
 * parameters registers.  The GEM_MAC_RX_ENABLE bit will read 0 when the
 * receiver has stopped.
 * The GEM_MAC_RX_HASH_FILTER bit must be cleared and a delay of 3.2ms 
 * imposed before writing to any of the Hash Table registers.  The
 * GEM_MAC_RX_HASH_FILTER bit will read 0 when the registers may be written.
 * The GEM_MAC_RX_ADDR_FILTER bit must be cleared and a delay of 3.2ms
 * imposed before writing to any of the Address Filter registers.  The
 * GEM_MAC_RX_ADDR_FILTER bit will read 0 when the registers may be written.
 * See "Carrier Extension" above.
 */
#define	GEM_MAC_RX_ENABLE	0x00000001	/* RX enable */
#define	GEM_MAC_RX_STRIP_PAD	0x00000002	/* strip pad bytes */
#define	GEM_MAC_RX_STRIP_CRC	0x00000004
#define	GEM_MAC_RX_PROMISCUOUS	0x00000008	/* promiscuous mode */
#define	GEM_MAC_RX_PROMISC_GRP	0x00000010	/* promiscuous group mode */
#define	GEM_MAC_RX_HASH_FILTER	0x00000020	/* enable hash filter */
#define	GEM_MAC_RX_ADDR_FILTER	0x00000040	/* enable address filter */
#define	GEM_MAC_RX_ERRCHK_DIS	0x00000080	/* disable error discard */
#define	GEM_MAC_RX_CARR_EXTEND	0x00000100	/* Ena RX Carrier Extension */
#define	GEM_MAC_RX_CONFIG_BITS	"\177\020" \
				"b\0RXENA\0b\1STRPAD\0b\2STRCRC\0" \
				"b\3PROMIS\0b\4PROMISCGRP\0b\5HASHFLTR\0" \
				"b\6ADDRFLTR\0b\7ERRCHKDIS\0b\x9TXCARREXT\0\0"


/*
 * Bits in GEM_MAC_CONTROL_CONFIG
 * Default; 0x0
 */
#define	GEM_MAC_CC_TX_PAUSE	0x00000001	/* send pause enabled */
#define	GEM_MAC_CC_RX_PAUSE	0x00000002	/* receive pause enabled */
#define	GEM_MAC_CC_PASS_PAUSE	0x00000004	/* pass pause up */
#define	GEM_MAC_CC_BITS		"\177\020b\0TXPAUSE\0b\1RXPAUSE\0b\2NOPAUSE\0\0"

/* GEM_MAC_MAC_STATE register bits */
#define GEM_MAC_STATE_OVERFLOW	0x03800000

/* 
 * Bits in GEM_MAC_SLOT_TIME register
 * The slot time is used as PAUSE time unit, value depends on whether carrier
 * extension is enabled.
 */
#define	GEM_MAC_SLOT_TIME_CARR_EXTEND	0x200
#define	GEM_MAC_SLOT_TIME_NORMAL	0x40


/*
 * Recommended values for MAC registers:
 *	GEM_MAC_IPG0	0x00
 *	GEM_MAC_IPG1	0x08
 *	GEM_MAC_IPG2	0x04
 *	GEM_MAC_SLOT_TIME	0x40		(see "Carrier Extension" above)
 *   Bits in GEM_MAC_MAC_MAX_FRAME register
 *   max burst size	0x7fff0000
 *   max frame size	0x00007fff
 *	GEM_MAC_MAC_MIN_FRAME	0x40
 *	GEM_MAC_MAC_MAX_FRAME	0x200005ee
 *	GEM_MAC_PREAMBLE_LEN	0x07		(minimum of 0x02)
 *	GEM_MAC_JAM_SIZE	0x04
 *	GEM_MAC_ATTEMPT_LIMIT	0x10
 *	GEM_MAC_CONTROL_TYPE	0x8808
 */


/*
 * Address detection and filtering registers (16-bit unless noted):
 *	GEM_MAC_ADDR0		normal priority MAC address bits 32-47
 *	GEM_MAC_ADDR1		normal priority MAC address bits 16-31
 *	GEM_MAC_ADDR2		normal priority MAC address bits 0-15
 *	GEM_MAC_ADDR3		alternate MAC address bits 32-47
 *	GEM_MAC_ADDR4		alternate MAC address bits 16-31
 *	GEM_MAC_ADDR5		alternate MAC address bits 0-15
 *	GEM_MAC_ADDR6		MAC control address bits 32-47
 *	GEM_MAC_ADDR7		MAC control address bits 16-31
 *	GEM_MAC_ADDR8		MAC control address bits 0-15
 *	GEM_MAC_ADDR_FILTER0	address filter bits 32-47
 *	GEM_MAC_ADDR_FILTER1	address filter bits 16-31
 *	GEM_MAC_ADDR_FILTER2	address filter bits 0-15
 *	GEM_MAC_ADR_FLT_MASK1_2	mask for GEM_MAC_ADDR_FILTER1 and 2 (8-bit)
 *	GEM_MAC_ADR_FLT_MASK0	mask for GEM_MAC_ADDR_FILTER0
 *	GEM_MAC_HASH0		hash table bits 240-255
 *	GEM_MAC_HASH1		hash table bits 224-239
 *	GEM_MAC_HASH2		hash table bits 208-223
 *	GEM_MAC_HASH3		hash table bits 192-207
 *	GEM_MAC_HASH4		hash table bits 176-191
 *	GEM_MAC_HASH5		hash table bits 160-175
 *	GEM_MAC_HASH6		hash table bits 144-159
 *	GEM_MAC_HASH7		hash table bits 128-143
 *	GEM_MAC_HASH8		hash table bits 112-127
 *	GEM_MAC_HASH9		hash table bits 96-111
 *	GEM_MAC_HASH10		hash table bits 80-95
 *	GEM_MAC_HASH11		hash table bits 64-79
 *	GEM_MAC_HASH12		hash table bits 48-63
 *	GEM_MAC_HASH13		hash table bits 32-47
 *	GEM_MAC_HASH14		hash table bits 16-31
 *	GEM_MAC_HASH15		hash table bits 0-15
 */
 
/*
 * Recommended values for statistic registers:
 *	GEM_MAC_NORM_COLL_CNT	0x0000
 *	GEM_MAC_FIRST_COLL_CNT	0x0000
 *	GEM_MAC_EXCESS_COLL_CNT	0x0000
 *	GEM_MAC_LATE_COLL_CNT	0x0000
 *	GEM_MAC_DEFER_TMR_CNT	0x0000
 *	GEM_MAC_PEAK_ATTEMPTS	0x0000
 *	GEM_MAC_RX_FRAME_COUNT	0x0000
 *	GEM_MAC_RX_LEN_ERR_CNT	0x0000
 *	GEM_MAC_RX_ALIGN_ERR	0x0000
 *	GEM_MAC_RX_CRC_ERR_CNT	0x0000
 *	GEM_MAC_RX_CODE_VIOL	0x0000
 */
		

/*
 * MIF Programmable Resources
 * Section 3.1.5.8
 * Bit-bang registers use low bit only
 */
#define	GEM_MIF_BB_CLOCK	0x6200		/* MIF Bit-Bang Clock */
#define	GEM_MIF_BB_DATA		0x6204		/* MIF Bit-Bang Data */
#define	GEM_MIF_BB_OUTPUT_ENAB	0x6208		/* MIF Bit-Bang Output Enable */
#define	GEM_MIF_FRAME		0x620c		/* MIF Frame/Output */
#define	GEM_MIF_CONFIG		0x6210		/* MIF Configuration */
#define	GEM_MIF_INTERRUPT_MASK	0x6214		/* MIF Mask */
#define	GEM_MIF_BASIC_STATUS	0x6218		/* MIF Status */
#define	GEM_MIF_STATE_MACHINE	0x621c		/* MIF State Machine */


/*
 * Bits in GEM_MIF_FRAME register
 */
#define	GEM_MIF_FRAME_DATA	0x0000ffff	/* Instruction payload */
#define	GEM_MIF_FRAME_TA0	0x00010000	/* TA bit, 1 for completion */
#define	GEM_MIF_FRAME_TA1	0x00020000	/* TA bits */
#define	GEM_MIF_FRAME_REG_ADDR	0x007c0000	/* Register address */
#define	GEM_MIF_FRAME_PHY_ADDR	0x0f800000	/* PHY address, should be 0 */
#define	GEM_MIF_FRAME_OP	0x30000000	/* operation - write/read */
#define	GEM_MIF_FRAME_START	0xc0000000	/* START bits */

#define	GEM_MIF_FRAME_READ	0x60020000
#define	GEM_MIF_FRAME_WRITE	0x50020000

#define	GEM_MIF_REG_SHIFT	18
#define	GEM_MIF_PHY_SHIFT	23


/*
 * Bits in GEM_MIF_CONFIG register
 */
#define	GEM_MIF_CONFIG_PHY_SEL	0x00000001	/* PHY select, 0=MDIO_0 */
#define	GEM_MIF_CONFIG_POLL_ENA	0x00000002	/* poll enable */
#define	GEM_MIF_CONFIG_BB_ENA	0x00000004	/* bit bang enable */
#define	GEM_MIF_CONFIG_REG_ADR	0x000000f8	/* poll register address */
#define	GEM_MIF_CONFIG_MDI0	0x00000100	/* MDIO_0 B-B data/attached */
#define	GEM_MIF_CONFIG_MDI1	0x00000200	/* MDIO_1 B-B data/attached */
#define	GEM_MIF_CONFIG_PHY_ADR	0x00007c00	/* poll PHY address */
/* MDIO_0 is onboard transceiver MDIO_1 is external, PHY addr for both is 0 */
#define	GEM_MIF_CONFIG_BITS	"\177\020b\0PHYSEL\0b\1POLL\0b\2BBENA\0" \
				"b\x8MDIO0\0b\x9MDIO1\0\0"


/*
 * Bits in GEM_MIF_BASIC_STATUS and GEM_MIF_INTERRUPT_MASK
 * The Basic part is the last value read in the POLL field of the config
 * register.
 * The status part indicates the bits that have changed.
 */
#define	GEM_MIF_STATUS		0x0000ffff
#define	GEM_MIF_BASIC		0xffff0000


/*
 * PCS/Serialink Registers
 * Section 3.1.6
 * DO NOT TOUCH THESE REGISTERS ON ERI -- IT HARD HANGS.
 */
#define	GEM_MII_CONTROL		0x9000		/* PCS MII Control */
#define	GEM_MII_STATUS		0x9004		/* PCS MII Status */
#define	GEM_MII_ANAR		0x9008		/* PCS MII Advertisement */
#define	GEM_MII_ANLPAR		0x900c		/* PCS MII Link Partner
						   Ability */
#define	GEM_MII_CONFIG		0x9010		/* PCS Configuration */
#define	GEM_MII_STATE_MACHINE	0x9014		/* PCS State Machine */
#define	GEM_MII_INTERRUP_STATUS	0x9018		/* PCS Interrupt Status */
#define	GEM_MII_DATAPATH_MODE	0x9050		/* Datapath Mode Register */
#define	GEM_MII_SLINK_CONTROL	0x9054		/* Serialink Control */
#define	GEM_MII_OUTPUT_SELECT	0x9058		/* Share Output Select */
#define	GEM_MII_SLINK_STATUS	0x905c		/* Serialink Status */


/*
 * Bits in GEM_MII_CONTROL register
 * PCS "BMCR" (Basic Mode Control Reg)
 * Default: 0x1040
 * AUTONEG and RESET self clear when relevant process is completed.
 */
#define GEM_MII_1GB_SPEED_SEL	0x00000040	/* 1000Mb/s, always 1 */
#define	GEM_MII_CONTROL_COL_TST	0x00000080	/* collision test */
#define	GEM_MII_CONTROL_FDUPLEX	0x00000100	/* full duplex, always 0 */
#define	GEM_MII_CONTROL_RAN	0x00000200	/* restart auto negotiation */
#define	GEM_MII_CONTROL_ISOLATE	0x00000400	/* isolate PHY, ignored */
#define	GEM_MII_CONTROL_POWERDN	0x00000800	/* power down, ignored */
#define	GEM_MII_CONTROL_AUTONEG	0x00001000	/* auto negotiation enabled */
#define	GEM_MII_CONTROL_SPEED	0x00002000	/* speed select, ignored */
#define	GEM_MII_CONTROL_LOOPBK	0x00004000	/* Serialink loopback */
#define	GEM_MII_CONTROL_RESET	0x00008000	/* Reset PCS */
#define	GEM_MII_CONTROL_BITS	"\177\020b\7COLTST\0b\x8_FD\0b\x9RAN\0" \
				"b\xaISOLATE\0b\xbPWRDWN\0b\xc_ANEG\0" \
				"b\xdGIGE\0b\xeLOOP\0b\xfRESET\0\0"


/*
 * Bits in GEM_MII_STATUS register.
 * PCS "BMSR" (Basic Mode Status Reg)
 * Default: 0x0108
 */
#define	GEM_MII_STATUS_EXTCAP	0x00000001	/* extended capability, always 0 */
#define	GEM_MII_STATUS_JABBER	0x00000002	/* jabber detected, always 0 */
#define	GEM_MII_STATUS_LINK_STS	0x00000004	/* link status, 1=up */
#define	GEM_MII_STATUS_ACFG	0x00000008	/* can auto neg, always 1 */
#define	GEM_MII_STATUS_REM_FLT	0x00000010	/* remote fault detected */
#define	GEM_MII_STATUS_ANEG_CPT	0x00000020	/* auto negotiate complete */
#define	GEM_MII_STATUS_EXT_STS	0x00000100	/* Is 1000Base-X, always 1 */
#define	GEM_MII_STATUS_GB_HDX	0x00000200	/* can perform GBit HDX */
#define	GEM_MII_STATUS_GB_FDX	0x00000400	/* can perform GBit FDX */
#define	GEM_MII_STATUS_BITS	"\177\020b\0EXTCAP\0b\1JABBER\0b\2LINKSTS\0" \
				"b\3ACFG\0b\4REMFLT\0b\5ANEGCPT\0b\x9GBHDX\0" \
				"b\xaGBFDX\0\0"


/*
 * Bits in GEM_MII_ANAR and GEM_MII_ANLPAR registers
 * GEM_MII_ANAR contains our capabilities for auto- negotiation
 * (Default: 0x00e0) and GEM_MII_ANLPAR contains the link partners
 * abilities and is only valid after auto-negotiation completes.
 */
#define	GEM_MII_ANEG_FUL_DUPLX	0x00000020	/* can do 1000Base-X FDX */
#define	GEM_MII_ANEG_HLF_DUPLX	0x00000040	/* can do 1000Base-X HDX */
#define	GEM_MII_ANEG_SYM_PAUSE	0x00000080	/* can do symmetric pause */
#define	GEM_MII_ANEG_ASYM_PAUSE	0x00000100	/* can do asymmetric pause */
#define	GEM_MII_ANEG_RF		0x00003000	/* advertise remote fault */
#define	GEM_MII_ANEG_ACK	0x00004000	/* ack reception of
						   Link Partner Capability */
#define	GEM_MII_ANEG_NP		0x00008000	/* next page bit, always 0 */
#define	GEM_MII_ANEG_BITS	"\177\020b\5FDX\0b\6HDX\0b\7SYMPAUSE\0" \
				"\b\x8_ASYMPAUSE\0\b\xdREMFLT\0\b\xeLPACK\0" \
				"\b\xfNPBIT\0\0"


/*
 * Bits in GEM_MII_CONFIG register
 * Default: 0x0
 * GEM_MII_CONFIG_ENABLE must be 0 when modifiying the GEM_MII_ANAR
 * register.  To isolate the MC from the media, set this bit to 0 and
 * restart auto-negotiation in GEM_MII_CONTROL.
 */
#define	GEM_MII_CONFIG_ENABLE	0x00000001	/* Enable PCS */
#define	GEM_MII_CONFIG_SDO	0x00000002	/* Signal Detect Override */
#define	GEM_MII_CONFIG_SDL	0x00000004	/* Signal Detect active low */
#define	GEM_MII_CONFIG_TIMER	0x0000000e	/* link monitor timer values */
#define	GEM_MII_CONFIG_JS	0x00000018	/* Jitter Study, 0 normal
						 * 1 high freq, 2 low freq */
#define	GEM_MII_CONFIG_ANTO	0x00000020	/* 10ms ANEG timer override */
#define	GEM_MII_CONFIG_BITS	"\177\020b\0PCSENA\0\0"


/*
 * Bits in GEM_MII_STATE_MACHINE register
 * XXX These are best guesses from observed behavior.
 */
#define	GEM_MII_FSM_STOP	0x00000000	/* stopped */
#define	GEM_MII_FSM_RUN		0x00000001	/* running */
#define	GEM_MII_FSM_UNKWN	0x00000100	/* unknown */
#define	GEM_MII_FSM_DONE	0x00000101	/* complete */


/*
 * Bits in GEM_MII_INTERRUP_STATUS register
 * No mask register; mask with the global interrupt mask register.
 */
#define	GEM_MII_INTERRUP_LINK	0x00000004	/* PCS link status change */


/*
 * Bits in GEM_MII_DATAPATH_MODE register
 * Default: none
 */
#define	GEM_MII_DATAPATH_SERIAL	0x00000001	/* Use internal Serialink */
#define	GEM_MII_DATAPATH_SERDES	0x00000002	/* Use PCS via 10bit interfac */
#define	GEM_MII_DATAPATH_MII	0x00000004	/* Use {G}MII, not PCS */
#define	GEM_MII_DATAPATH_MIIOUT	0x00000008	/* Set serial output on GMII */
#define GEM_MII_DATAPATH_BITS	"\177\020"				\
				"b\0SERIAL\0b\1SERDES\0b\2MII\0b\3MIIOUT\0\0"


/*
 * Bits in GEM_MII_SLINK_CONTROL register
 * Default: 0x000
 */
#define	GEM_MII_SLINK_LOOPBACK	0x00000001	/* enable loopback on Serialink
						   disable loopback on SERDES */
#define	GEM_MII_SLINK_EN_SYNC_D	0x00000002	/* enable sync detection */
#define	GEM_MII_SLINK_LOCK_REF	0x00000004	/* lock reference clock */
#define	GEM_MII_SLINK_EMPHASIS	0x00000018	/* enable emphasis */
#define	GEM_MII_SLINK_SELFTEST	0x000001c0
#define	GEM_MII_SLINK_POWER_OFF	0x00000200	/* Power down Serialink block */
#define	GEM_MII_SLINK_RX_ZERO	0x00000c00	/* PLL input to Serialink */
#define	GEM_MII_SLINK_RX_POLL	0x00003000	/* PLL input to Serialink */
#define	GEM_MII_SLINK_TX_ZERO	0x0000c000	/* PLL input to Serialink */
#define	GEM_MII_SLINK_TX_POLL	0x00030000	/* PLL input to Serialink */
#define	GEM_MII_SLINK_CONTROL_BITS					\
				"\177\020b\0LOOP\0b\1ENASYNC\0b\2LOCKREF" \
				"\0b\3EMPHASIS1\0b\4EMPHASIS2\0b\x9PWRDWN\0\0"


/*
 * Bits in GEM_MII_OUTPUT_SELECT register
 * Default: 0x0
 */
#define GEM_MII_PROM_ADDR	0x00000003	/* Test output multiplexor */


/*
 * Bits in GEM_MII_SLINK_STATUS register
 * Default: 0x0
 */
#define	GEM_MII_SLINK_TEST	0x00000000	/* undergoing test */
#define	GEM_MII_SLINK_LOCKED	0x00000001	/* waiting 500us lockrefn */
#define	GEM_MII_SLINK_COMMA	0x00000002	/* waiting for comma detect */
#define	GEM_MII_SLINK_SYNC	0x00000003	/* recv data synchronized */


/*
 * PCI Expansion ROM runtime access
 * Sun GEMs map a 1MB space for the PCI Expansion ROM as the second half
 * of the first register bank, although they only support up to 64KB ROMs.
 */
#define	GEM_PCI_ROM_OFFSET	0x100000
#define	GEM_PCI_ROM_SIZE	0x10000


/* Wired GEM PHY addresses */
#define	GEM_PHYAD_INTERNAL	1
#define	GEM_PHYAD_EXTERNAL	0

/*
 * GEM descriptor table structures.
 */
struct gem_desc {
	volatile uint64_t	gd_flags;
	volatile uint64_t	gd_addr;
};

/* Transmit flags */
#define	GEM_TD_BUFSIZE		0x0000000000007fffLL
#define	GEM_TD_CXSUM_START	0x00000000001f8000LL	/* Cxsum start offset */
#define	GEM_TD_CXSUM_STARTSHFT	15
#define	GEM_TD_CXSUM_STUFF	0x000000001fe00000LL	/* Cxsum stuff offset */
#define	GEM_TD_CXSUM_STUFFSHFT	21
#define	GEM_TD_CXSUM_ENABLE	0x0000000020000000LL	/* Cxsum generation enable */
#define	GEM_TD_END_OF_PACKET	0x0000000040000000LL
#define	GEM_TD_START_OF_PACKET	0x0000000080000000LL
#define	GEM_TD_INTERRUPT_ME	0x0000000100000000LL	/* Interrupt me now */
#define	GEM_TD_NO_CRC		0x0000000200000000LL	/* do not insert crc */
/*
 * Only need to set GEM_TD_CXSUM_ENABLE, GEM_TD_CXSUM_STUFF,
 * GEM_TD_CXSUM_START, and GEM_TD_INTERRUPT_ME in 1st descriptor of a group.
 */

/* Receive flags */
#define	GEM_RD_CHECKSUM		0x000000000000ffffLL	/* is the complement */
#define	GEM_RD_BUFSIZE		0x000000007fff0000LL
#define	GEM_RD_OWN		0x0000000080000000LL	/* 1 - owned by h/w */
#define	GEM_RD_HASHVAL		0x0ffff00000000000LL
#define	GEM_RD_HASH_PASS	0x1000000000000000LL	/* passed hash filter */
#define	GEM_RD_ALTERNATE_MAC	0x2000000000000000LL	/* Alternate MAC adrs */
#define	GEM_RD_BAD_CRC		0x4000000000000000LL

#define	GEM_RD_BUFSHIFT		16
#define	GEM_RD_BUFLEN(x)	(((x)&GEM_RD_BUFSIZE)>>GEM_RD_BUFSHIFT)

#endif
