/*	$NetBSD: elink3reg.h,v 1.31 2006/11/05 05:57:53 itohy Exp $	*/

/*
 * Copyright (c) 1995 Herb Peyerl <hpeyerl@beer.org>
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
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 */

/*
 * These define the EEPROM data structure.  They are used in the probe
 * function to verify the existence of the adapter after having sent
 * the ID_Sequence.
 */
#define EEPROM_NODE_ADDR_0	0x0	/* Word */
#define EEPROM_NODE_ADDR_1	0x1	/* Word */
#define EEPROM_NODE_ADDR_2	0x2	/* Word */
#define EEPROM_PROD_ID		0x3	/* 0x9[0-f]50 */
#define EEPROM_MFG_DATE		0x4	/* Manufacturing date */
#define EEPROM_MFG_DIVSION	0x5	/* Manufacturing division */
#define EEPROM_MFG_PRODUCT	0x6	/* Product code */
#define EEPROM_MFG_ID		0x7	/* 0x6d50 */
#define EEPROM_ADDR_CFG		0x8	/* Base addr */
#define EEPROM_RESOURCE_CFG	0x9     /* IRQ. Bits 12-15 */
#define EEPROM_OEM_ADDR0	0xa
#define EEPROM_OEM_ADDR1	0xb
#define EEPROM_OEM_ADDR2	0xc
#define EEPROM_SOFTINFO		0xd
#define EEPROM_COMPAT		0xe
#define EEPROM_SOFTINFO2	0xf
#define EEPROM_CAP		0x10
#define EEPROM_CONFIG_LOW	0x12
#define EEPROM_CONFIG_HIGH	0x13
#define EEPROM_SSI		0x14
#define EEPROM_CHECKSUM_EL3	0x17

/*
 * These are the registers for the 3Com 3c509 and their bit patterns when
 * applicable.  They have been taken out of the "EtherLink III Parallel
 * Tasking EISA and ISA Technical Reference" "Beta Draft 10/30/92" manual
 * from 3com.
 */
#define ELINK_COMMAND	0x0e    /* Write. BASE+0x0e is always a command reg. */
#define ELINK_STATUS	0x0e    /* Read. BASE+0x0e is always status reg. */
#define ELINK_WINDOW	0x0f    /* Read. BASE+0x0f is always window reg. */

/*
 * Corkscrew ISA Bridge ASIC registers.
 */
#define	CORK_ASIC_DCR		0x2000
#define	CORK_ASIC_RCR		0x2002
#define	CORK_ASIC_ROM_PAGE	0x2004	/* 8-bit */
#define	CORK_ASIC_MCR		0x2006	/* 8-bit */
#define	CORK_ASIC_DEBUG		0x2008	/* 8-bit */
#define	CORK_ASIC_EEPROM_COMMAND 0x200a
#define	CORK_ASIC_EEPROM_DATA	0x200c

/*
 * Corkscrew DMA Control Register.
 */
#define	DCR_CHRDYWAIT_40ns	(0 << 13)
#define	DCR_CHRDYWAIT_80ns	(1 << 13)
#define	DCR_CHRDYWAIT_120ns	(2 << 13)
#define	DCR_CHRDYWAIT_160ns	(3 << 13)
#define	DCR_RECOVWAIT_80ns	(0 << 11)
#define	DCR_RECOVWAIT_120ns	(1 << 11)
#define	DCR_RECOVWAIT_160ns	(2 << 11)
#define	DCR_RECOVWAIT_200ns	(3 << 11)
#define	DCR_CMDWIDTH(x)		((x) << 8) /* base 40ns, 40ns increments */
#define	DCR_BURSTLEN(x)		((x) << 3)
#define	DCR_DRQSELECT(x)	((x) << 0) /* 3, 5, 6, 7 valid */

/*
 * Corkscrew Debug register.
 */
#define	DEBUG_PCIBUSFAULT	(1U << 0)
#define	DEBUG_ISABUSFAULT	(1U << 1)

/*
 * Corkscrew EEPROM Command register.
 */
#define	CORK_EEPROM_BUSY	(1U << 9)
#define	CORK_EEPROM_CMD_READ	(1U << 7)	/* Same as 3c509 */

/*
 * Corkscrew Master Control register.
 */
#define	MCR_PCI_CONFIG		(1U << 0)
#define	MCR_WRITE_BUFFER	(1U << 1)
#define	MCR_READ_PREFETCH	(1U << 2)

/*
 * Window 0 registers. Setup.
 */
	/* Write */
#define ELINK_W0_EEPROM_DATA	0x0c
#define ELINK_W0_EEPROM_COMMAND	0x0a
#define ELINK_W0_RESOURCE_CFG	0x08
#define ELINK_W0_ADDRESS_CFG	0x06
#define ELINK_W0_CONFIG_CTRL	0x04
	/* Read */
#define ELINK_W0_PRODUCT_ID	0x02
#define ELINK_W0_MFG_ID		0x00

/*
 * Window 1 registers. Operating Set.
 */
	/* Write */
#define ELINK_W1_TX_PIO_WR_2	0x02
#define ELINK_W1_TX_PIO_WR_1	0x00
	/* Read */
#define ELINK_W1_FREE_TX	0x0c
#define ELINK_W1_TX_STATUS	0x0a
#define ELINK_W1_RX_STATUS	0x08
#define	ELINK_W1_RX_ERRORS	0x04
#define ELINK_W1_RX_PIO_RD_2	0x02
#define ELINK_W1_RX_PIO_RD_1	0x00
/*
 * Special registers used by the RoadRunner.  These are used to program
 * a FIFO buffer to reduce the PCMCIA->PCI bridge latency during PIO.
 */
#define	ELINK_W1_RUNNER_RDCTL	0x16
#define	ELINK_W1_RUNNER_WRCTL	0x1c

/*
 * Window 2 registers. Station Address Setup/Read
 */
	/* Read/Write */
#define ELINK_W2_RECVMASK_0	0x06
#define ELINK_W2_ADDR_5		0x05
#define ELINK_W2_ADDR_4		0x04
#define ELINK_W2_ADDR_3		0x03
#define ELINK_W2_ADDR_2		0x02
#define ELINK_W2_ADDR_1		0x01
#define ELINK_W2_ADDR_0		0x00

/*
 * Window 3 registers.  Configuration and FIFO Management.
 */
	/* Read */
#define ELINK_W3_FREE_TX		0x0c
#define ELINK_W3_FREE_RX		0x0a
	/* Read/Write, at least on busmastering cards. */
#define ELINK_W3_INTERNAL_CONFIG	0x00	/* 32 bits */
#define ELINK_W3_OTHER_INT		0x04	/*  8 bits */
#define ELINK_W3_PIO_RESERVED	0x05	/*  8 bits */
#define ELINK_W3_MAC_CONTROL	0x06	/* 16 bits */
#define ELINK_W3_RESET_OPTIONS	0x08	/* 16 bits */

/*
 * Window 4 registers. Diagnostics.
 */
	/* Read/Write */
#define ELINK_W4_MEDIA_TYPE		0x0a
#define ELINK_W4_CTRLR_STATUS		0x08
#define ELINK_W4_NET_DIAG		0x06
#define ELINK_W4_FIFO_DIAG		0x04
#define ELINK_W4_HOST_DIAG		0x02
#define ELINK_W4_TX_DIAG		0x00

/*
 * Window 4 offset 8 is the PHY Management register on the
 * 3c90x.
 */
#define	ELINK_W4_BOOM_PHYSMGMT	0x08
#define	PHYSMGMT_CLK		0x0001
#define	PHYSMGMT_DATA		0x0002
#define	PHYSMGMT_DIR		0x0004


/*
 * Window 5 Registers.  Results and Internal status.
 */
	/* Read */
#define ELINK_W5_READ_0_MASK	0x0c
#define ELINK_W5_INTR_MASK		0x0a
#define ELINK_W5_RX_FILTER		0x08
#define ELINK_W5_RX_EARLY_THRESH	0x06
#define ELINK_W5_TX_AVAIL_THRESH	0x02
#define ELINK_W5_TX_START_THRESH	0x00

/*
 * Window 6 registers. Statistics.
 */
	/* Read/Write */
#define TX_TOTAL_OK		0x0c
#define RX_TOTAL_OK		0x0a
#define UPPER_FRAMES_OK		0x09
#define TX_DEFERRALS		0x08
#define RX_FRAMES_OK		0x07
#define TX_FRAMES_OK		0x06
#define RX_OVERRUNS		0x05
#define TX_COLLISIONS		0x04
#define TX_AFTER_1_COLLISION	0x03
#define TX_AFTER_X_COLLISIONS	0x02
#define TX_NO_SQE		0x01
#define TX_CD_LOST		0x00

/*
 * Window 7 registers.
 * Address and length for a single bus-master DMA transfer.
 * Unused for elink3 cards.
 */
#define ELINK_W7_MASTER_ADDRES	0x00
#define ELINK_W7_RX_ERROR	0x04
#define ELINK_W7_MASTER_LEN	0x06
#define ELINK_W7_RX_STATUS	0x08
#define ELINK_W7_MASTER_STATUS	0x0c

/*
 * Register definitions.
 */

/*
 * Command register. All windows.
 *
 * 16 bit register.
 *     15-11:  5-bit code for command to be executed.
 *     10-0:   11-bit arg if any. For commands with no args;
 *	      this can be set to anything.
 */
/* Wait at least 1ms after issuing */
#define GLOBAL_RESET		(u_int16_t) 0x0000
#define WINDOW_SELECT		(u_int16_t) (0x01<<11)
/*
 * Read ADDR_CFG reg to determine whether this is needed. If so; wait 800
 * uSec before using transceiver.
 */
#define START_TRANSCEIVER	(u_int16_t) (0x02<<11)
/* state disabled on power-up */
#define RX_DISABLE		(u_int16_t) (0x03<<11)
#define RX_ENABLE		(u_int16_t) (0x04<<11)
#define RX_RESET		(u_int16_t) (0x05<<11)
#define RX_DISCARD_TOP_PACK	(u_int16_t) (0x08<<11)
#define TX_ENABLE		(u_int16_t) (0x09<<11)
#define TX_DISABLE		(u_int16_t) (0x0a<<11)
#define TX_RESET		(u_int16_t) (0x0b<<11)
#define REQ_INTR		(u_int16_t) (0x0c<<11)
#define ACK_INTR		(u_int16_t) (0x0d<<11)
#define SET_INTR_MASK		(u_int16_t) (0x0e<<11)
/* busmastering-cards only? */
#define STATUS_ENABLE		(u_int16_t) (0x0f<<11)
#define SET_RD_0_MASK		(u_int16_t) (0x0f<<11)

#define SET_RX_FILTER		(u_int16_t) (0x10<<11)
#      define FIL_INDIVIDUAL	(u_int16_t) (0x01)
#      define FIL_MULTICAST	(u_int16_t) (0x02)
#      define FIL_BRDCST	(u_int16_t) (0x04)
#      define FIL_PROMISC	(u_int16_t) (0x08)

#define SET_RX_EARLY_THRESH	(u_int16_t) (0x11<<11)
#define SET_TX_AVAIL_THRESH	(u_int16_t) (0x12<<11)
#define SET_TX_START_THRESH	(u_int16_t) (0x13<<11)
#define START_DMA		(u_int16_t) (0x14<<11)	/* busmaster-only */
#  define START_DMA_TX		(START_DMA | 0x0))	/* busmaster-only */
#  define START_DMA_RX		(START_DMA | 0x1)	/* busmaster-only */
#define STATS_ENABLE		(u_int16_t) (0x15<<11)
#define STATS_DISABLE		(u_int16_t) (0x16<<11)
#define STOP_TRANSCEIVER	(u_int16_t) (0x17<<11)

/* Only on adapters that support power management: */
#define POWERUP			(u_int16_t) (0x1b<<11)
#define POWERDOWN		(u_int16_t) (0x1c<<11)
#define POWERAUTO		(u_int16_t) (0x1d<<11)



/*
 * Command parameter that disables threshold interrupts
 *   PIO (3c509) cards use 2044.  The fifo word-oriented and 2044--2047 work.
 *  "busmastering" cards need 8188.
 * The implicit two-bit upshift done by busmastering cards means
 * a value of 2047 disables threshold interrupts on both.
 */
#define ELINK_THRESH_DISABLE	2047


/*
 * Status register. All windows.
 *
 *     15-13:  Window number(0-7).
 *     12:     Command_in_progress.
 *     11:     reserved / DMA in progress on busmaster cards.
 *     10:     reserved.
 *     9:      reserved.
 *     8:      reserved / DMA done on busmaster cards.
 *     7:      Update Statistics.
 *     6:      Interrupt Requested.
 *     5:      RX Early.
 *     4:      RX Complete.
 *     3:      TX Available.
 *     2:      TX Complete.
 *     1:      Adapter Failure.
 *     0:      Interrupt Latch.
 */
#define INTR_LATCH		(u_int16_t) (0x0001)
#define CARD_FAILURE		(u_int16_t) (0x0002)
#define TX_COMPLETE		(u_int16_t) (0x0004)
#define TX_AVAIL		(u_int16_t) (0x0008)
#define RX_COMPLETE		(u_int16_t) (0x0010)
#define RX_EARLY		(u_int16_t) (0x0020)
#define INT_RQD			(u_int16_t) (0x0040)
#define UPD_STATS		(u_int16_t) (0x0080)
#define DMA_DONE		(u_int16_t) (0x0100)	/* DMA cards only */
#define DMA_IN_PROGRESS		(u_int16_t) (0x0800)	/* DMA cards only */
#define COMMAND_IN_PROGRESS	(u_int16_t) (0x1000)

#define ALL_INTERRUPTS	(CARD_FAILURE | TX_COMPLETE | TX_AVAIL | RX_COMPLETE | \
    RX_EARLY | INT_RQD | UPD_STATS)

#define WATCHED_INTERRUPTS (CARD_FAILURE | TX_COMPLETE | RX_COMPLETE | TX_AVAIL)

/*
 * FIFO Registers.  RX Status.
 *
 *     15:     Incomplete or FIFO empty.
 *     14:     1: Error in RX Packet   0: Incomplete or no error.
 *     14-11:  Type of error. [14-11]
 *	      1000 = Overrun.
 *	      1011 = Run Packet Error.
 *	      1100 = Alignment Error.
 *	      1101 = CRC Error.
 *	      1001 = Oversize Packet Error (>1514 bytes)
 *	      0010 = Dribble Bits.
 *	      (all other error codes, no errors.)
 *
 *     10-0:   RX Bytes (0-1514)
 */
#define ERR_INCOMPLETE  (u_int16_t) (0x8000)
#define ERR_RX		(u_int16_t) (0x4000)
#define ERR_MASK	(u_int16_t) (0x7800)
#define ERR_OVERRUN	(u_int16_t) (0x4000)
#define ERR_RUNT	(u_int16_t) (0x5800)
#define ERR_ALIGNMENT	(u_int16_t) (0x6000)
#define ERR_CRC		(u_int16_t) (0x6800)
#define ERR_OVERSIZE	(u_int16_t) (0x4800)
#define ERR_DRIBBLE	(u_int16_t) (0x1000)

/*
 * TX Status
 *
 *   Reports the transmit status of a completed transmission. Writing this
 *   register pops the transmit completion stack.
 *
 *   Window 1/Port 0x0b.
 *
 *     7:      Complete
 *     6:      Interrupt on successful transmission requested.
 *     5:      Jabber Error (TP Only, TX Reset required. )
 *     4:      Underrun (TX Reset required. )
 *     3:      Maximum Collisions.
 *     2:      TX Status Overflow.
 *     1-0:    Undefined.
 *
 */
#define TXS_COMPLETE		0x8000
#define TXS_INTR_REQ		0x4000
#define TXS_JABBER		0x2000
#define TXS_UNDERRUN		0x1000
#define TXS_MAX_COLLISION	0x0800
#define TXS_STATUS_OVERFLOW	0x0400
#define TXS_RECLAIM		0x0200
#define	TXS_TIMER		0x00ff

/*
 * RX status
 *   Window 1/Port 0x08.
 */
#define RX_BYTES_MASK			(u_int16_t) (0x07ff)

/*
 * Internal Config and MAC control (Window 3)
 * Window 3 / Port 0: 32-bit internal config register:
 * bits  0-2:    fifo buffer ram  size
 *         3:    ram width (word/byte)     (ro)
 *       4-5:    ram speed
 *       6-7:    rom size
 *      8-15:   reserved
 *
 *     16-17:   ram split (5:3, 3:1, or 1:1).
 *     18-19:   reserved
 *     20-22:   selected media type
 *        21:   unused
 *        24:  (nonvolatile) driver should autoselect media
 *     25-31: reserved
 *
 * The low-order 16 bits should generally not be changed by software.
 * Offsets defined for two 16-bit words, to help out 16-bit busses.
 */
#define	CONFIG_RAMSIZE		(u_int16_t) 0x0007
#define	CONFIG_RAMSIZE_SHIFT	0

#define	CONFIG_RAMWIDTH		(u_int16_t) 0x0008
#define	CONFIG_RAMWIDTH_SHIFT	3

#define	CONFIG_RAMSPEED		(u_int16_t) 0x0030
#define	CONFIG_RAMSPEED_SHIFT	4
#define	CONFIG_ROMSIZE		(u_int16_t) 0x00c0
#define	CONFIG_ROMSIZE_SHIFT	6

/* Window 3/port 2 */
#define	CONFIG_RAMSPLIT		(u_int16_t) 0x0003
#define	CONFIG_RAMSPLIT_SHIFT	0
#define	CONFIG_MEDIAMASK	(u_int16_t) 0x0070
#define	CONFIG_MEDIAMASK_SHIFT	4

#define	CONFIG_AUTOSELECT	(u_int16_t) 0x0100
#define	CONFIG_AUTOSELECT_SHIFT	8

/*
 * MAC_CONTROL (Window 3)
 */
#define MAC_CONTROL_FDX		0x20    /* full-duplex mode */


/* Active media in INTERNAL_CONFIG media bits */

#define ELINKMEDIA_10BASE_T		(u_int16_t)   0x00
#define ELINKMEDIA_AUI			(u_int16_t)   0x01
#define ELINKMEDIA_RESV1		(u_int16_t)   0x02
#define ELINKMEDIA_10BASE_2		(u_int16_t)   0x03
#define ELINKMEDIA_100BASE_TX		(u_int16_t)   0x04
#define ELINKMEDIA_100BASE_FX		(u_int16_t)   0x05
#define ELINKMEDIA_MII			(u_int16_t)   0x06
#define ELINKMEDIA_100BASE_T4		(u_int16_t)   0x07


/*
 * RESET_OPTIONS (Window 3, on Demon/Vortex/Boomerang only)
 * also mapped to PCI configuration space on PCI adaptors.
 *
 * (same register as  Vortex ELINK_W3_RESET_OPTIONS, mapped to pci-config space)
 */
#define ELINK_PCI_100BASE_T4		(1<<0)
#define ELINK_PCI_100BASE_TX		(1<<1)
#define ELINK_PCI_100BASE_FX		(1<<2)
#define ELINK_PCI_10BASE_T			(1<<3)
#define ELINK_PCI_BNC			(1<<4)
#define ELINK_PCI_AUI 			(1<<5)
#define ELINK_PCI_100BASE_MII		(1<<6)
#define ELINK_PCI_INTERNAL_VCO		(1<<8)

#define	ELINK_PCI_MEDIAMASK	(ELINK_PCI_100BASE_T4|ELINK_PCI_100BASE_TX| \
				 ELINK_PCI_100BASE_FX|ELINK_PCI_10BASE_T| \
				 ELINK_PCI_BNC|ELINK_PCI_AUI| \
				 ELINK_PCI_100BASE_MII)

#define	ELINK_RUNNER_MII_RESET		0x4000
#define	ELINK_RUNNER_ENABLE_MII		0x8000

/*
 * FIFO Status (Window 4)
 *
 *   Supports FIFO diagnostics
 *
 *   Window 4/Port 0x04.1
 *
 *     15:	1=RX receiving (RO). Set when a packet is being received
 *		into the RX FIFO.
 *     14:	Reserved
 *     13:	1=RX underrun (RO). Generates Adapter Failure interrupt.
 *		Requires RX Reset or Global Reset command to recover.
 *		It is generated when you read past the end of a packet -
 *		reading past what has been received so far will give bad
 *		data.
 *     12:	1=RX status overrun (RO). Set when there are already 8
 *		packets in the RX FIFO. While this bit is set, no additional
 *		packets are received. Requires no action on the part of
 *		the host. The condition is cleared once a packet has been
 *		read out of the RX FIFO.
 *     11:	1=RX overrun (RO). Set when the RX FIFO is full (there
 *		may not be an overrun packet yet). While this bit is set,
 *		no additional packets will be received (some additional
 *		bytes can still be pending between the wire and the RX
 *		FIFO). Requires no action on the part of the host. The
 *		condition is cleared once a few bytes have been read out
 *		from the RX FIFO.
 *     10:	1=TX overrun (RO). Generates adapter failure interrupt.
 *		Requires TX Reset or Global Reset command to recover.
 *		Disables Transmitter.
 *     9-8:	Unassigned.
 *     7-0:	Built in self test bits for the RX and TX FIFO's.
 */
#define	FIFOS_RX_RECEIVING	(u_int16_t) 0x8000
#define	FIFOS_RX_UNDERRUN	(u_int16_t) 0x2000
#define	FIFOS_RX_STATUS_OVERRUN	(u_int16_t) 0x1000
#define	FIFOS_RX_OVERRUN	(u_int16_t) 0x0800
#define	FIFOS_TX_OVERRUN	(u_int16_t) 0x0400

/*
 * ISA/eisa CONFIG_CNTRL media-present bits.
 */
#define ELINK_W0_CC_AUI 			(1<<13)
#define ELINK_W0_CC_BNC 			(1<<12)
#define ELINK_W0_CC_UTP 			(1<<9)
#define	ELINK_W0_CC_MEDIAMASK	(ELINK_W0_CC_AUI|ELINK_W0_CC_BNC| \
				 ELINK_W0_CC_UTP)

/* EEPROM state flags/commands */
#define EEPROM_BUSY			(1<<15)
#define EEPROM_TST_MODE			(1<<14)

#define READ_EEPROM			(1<<7)

	/* For the RoadRunner chips... */
#define	WRITE_EEPROM_RR			0x100
#define	READ_EEPROM_RR			0x200
#define	ERASE_EEPROM_RR			0x300

/* window 4, MEDIA_STATUS bits */
#define SQE_ENABLE			0x08	/* Enables SQE on AUI ports */
#define JABBER_GUARD_ENABLE		0x40
#define LINKBEAT_ENABLE			0x80
#define DISABLE_UTP			0x0
#define LINKBEAT_DETECT			0x800

/*
 * Misc defines for various things.
 */
#define TAG_ADAPTER 			0xd0
#define ACTIVATE_ADAPTER_TO_CONFIG 	0xff
#define ENABLE_DRQ_IRQ			0x0001
#define MFG_ID				0x506d	/* `TCM' */
#define PROD_ID_3C509			0x5090	/* 509[0-f] */
#define GO_WINDOW(x) 			bus_space_write_2(sc->sc_iot, \
				sc->sc_ioh, ELINK_COMMAND, WINDOW_SELECT|x)


/* Used to probe for large-packet support. */
#define ELINK_LARGEWIN_PROBE		ELINK_THRESH_DISABLE
#define ELINK_LARGEWIN_MASK		0xffc
