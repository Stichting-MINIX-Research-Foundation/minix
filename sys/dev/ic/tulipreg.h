/*	$NetBSD: tulipreg.h,v 1.37 2012/01/16 17:58:02 jakllsch Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _DEV_IC_TULIPREG_H_
#define	_DEV_IC_TULIPREG_H_

/*
 * Register description for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family, and a variety of clone chips, including:
 *
 *	- Macronix 98713, 98713A, 98715, 98715A, 98725 (PMAC):
 *
 *	  These chips are fairly straight-forward Tulip clones.
 *	  The 98713 is a very close 21140A clone.  It has GPR
 *	  and MII media, and a GPIO facility, and uses the ISV
 *	  SROM format (or, at least, should, because of the GPIO
 *	  facility).  The 98713A has MII, no GPIO facility, and
 *	  an internal NWay block.  The 98715, 98715A, and 98725
 *	  have only GPR media and the NWay block.  The 98715,
 *	  98715A, and 98725 support power management.
 *
 *        The 98715AEC adds 802.3x flow Frame based Flow Control to the
 *	  98715A.
 *
 *	- Lite-On 82C115 (PNIC II):
 *
 *	  A clone of the Macronix MX98725, with the following differences:
 *
 *		- Wake-On-LAN support
 *		- 128-bit multicast hash table rather than the
 *		  standard 512-bit hash table
 *		- 802.3x flow control
 *
 *	- Lite-On 82C168, 82C169 (PNIC):
 *
 *	  Pretty close, with only a few minor differences:
 *
 *		- EEPROM is accessed completely differently.
 *		- MII is accessed completely differently.
 *		- No SIO facility (due to the above two differences).
 *		- GPIO interface is different than the 21140's.
 *		- Boards that lack PHYs use the internal NWay block
 *		  and transceiver.
 *
 *	- Winbond 89C840F
 *
 *	  Less similar, but still roughly compatible (enough so
 *	  that the driver can be adapted, at least):
 *
 *		- Registers lack the pad word between them.
 *		- Instead of a setup frame, there are two station
 *		  address registers and two multicast hash table
 *		  registers (64-bit multicast hash table).
 *		- Only supported media interface is MII-over-SIO.
 *		- Different OPMODE register bits for various things
 *		  (mostly media related).
 *
 *	- ADMtek AL981
 *
 *	  Another pretty-close clone:
 *
 *		- Wake-On-LAN support
 *		- Instead of a setup frame, there are two station
 *		  address registers and two multicast hash table
 *		  registers (64-bit multicast hash table).
 *		- 802.3x flow control
 *		- Only supported media interface is built-in PHY
 *		  which is accessed through a set of special registers.
 *		- Not all registers have the pad word between them,
 *		  but luckily, there are all AL981-specific registers,
 *		  so this is easy to deal with.
 *
 *	- ADMtek AN983 and AN985
 *
 *	  Similar to the ADMtek AL981, but with a few differences.
 *
 *	- Xircom X3201-3
 *
 *	  CardBus 21143 clone, with a few differences:
 *
 *		- No MicroWire SROM; Ethernet address must come
 *		  from CIS.
 *		- Transmit buffers must also be 32-bit aligned.
 *		- The BUSMODE_SWR bit is not self-clearing.
 *		- Must include FS|LS in setup packet descriptor.
 *		- SIA is not 21143-like, and all media attachments
 *		  are MII-on-SIO.
 *
 *	- Davicom DM9102 and DM9102A
 *
 *	  Pretty similar to the 21140A, with a few differences:
 *
 *		- Wake-On-LAN support
 *		- DM9102 has built-in 10/100 PHY on MII interface.
 *		- DM9102A has built-in 10/100 PHY on MII interface,
 *		  as well as a HomePNA 1 PHY on an alternate MII
 *		  interface (selected by clearing OPMODE_PS).
 *		- The chip has a bug in the transmit DMA logic,
 *		  requiring that the packet be comprised of only
 *		  one DMA segment.
 *		- The bus interface is buggy, and the BUSMODE register
 *		  must be initialized to 0.
 *		- There seems to be an interrupt logic bug, requiring
 *		  that interrupts be disabled on the chip during the
 *		  interrupt handler.
 *	
 *	- ASIX AX88140
 *	
 *	  21433 clone with a few differences:
 *
 *	  	- Specific broadcast bit in the OPMODE register.
 *	  	- Transmit buffer must be 32-bit aligned.
 *	  	- The BUSMODE_SWR bit is not self-clearing.
 *	  	- External 10BaseT PHY or 10/100 MII.
 *
 * Some of the clone chips have different registers, and some have
 * different bits in the same registers.  These will be denoted by
 * PMAC, PNICII, PNIC, DM, WINB, ADM and AX in the register/bit names.
 */

/*
 * Tulip buffer descriptor.  Must be 4-byte aligned.
 *
 * Note for receive descriptors, the byte count fields must
 * be a multiple of 4.
 */
struct tulip_desc {
	volatile uint32_t td_status;	  /* Status */
	volatile uint32_t td_ctl;	  /* Control and Byte Counts */
	volatile uint32_t td_bufaddr1; /* Buffer Address 1 */
	volatile uint32_t td_bufaddr2; /* Buffer Address 2 */
} __packed __aligned(4);

/*
 * Descriptor Status bits common to transmit and receive.
 */
#define	TDSTAT_OWN	0x80000000	/* Tulip owns descriptor */
#define	TDSTAT_ES	0x00008000	/* Error Summary */

/*
 * Descriptor Status bits for Receive Descriptor.
 */
#define	TDSTAT_Rx_FF	0x40000000	/* Filtering Fail */
#define	TDSTAT_WINB_Rx_RCMP 0x40000000	/* Receive Complete */
#define	TDSTAT_Rx_FL	0x3fff0000	/* Frame Length including CRC */
#define	TDSTAT_Rx_DE	0x00004000	/* Descriptor Error */
#define	TDSTAT_Rx_DT	0x00003000	/* Data Type */
#define	TDSTAT_Rx_RF	0x00000800	/* Runt Frame */
#define	TDSTAT_Rx_MF	0x00000400	/* Multicast Frame */
#define	TDSTAT_Rx_FS	0x00000200	/* First Descriptor */
#define	TDSTAT_Rx_LS	0x00000100	/* Last Descriptor */
#define	TDSTAT_Rx_TL	0x00000080	/* Frame Too Long */
#define	TDSTAT_Rx_CS	0x00000040	/* Collision Seen */
#define	TDSTAT_Rx_RT	0x00000020	/* Frame Type */
#define	TDSTAT_Rx_RW	0x00000010	/* Receive Watchdog */
#define	TDSTAT_Rx_RE	0x00000008	/* Report on MII Error */
#define	TDSTAT_Rx_DB	0x00000004	/* Dribbling Bit */
#define	TDSTAT_Rx_CE	0x00000002	/* CRC Error */
#define	TDSTAT_Rx_ZER	0x00000001	/* Zero (always 0) */

#define	TDSTAT_Rx_LENGTH(x)	(((x) & TDSTAT_Rx_FL) >> 16)

#define	TDSTAT_Rx_DT_SR	0x00000000	/* Serial Received Frame */
#define	TDSTAT_Rx_DT_IL	0x00001000	/* Internal Loopback Frame */
#define	TDSTAT_Rx_DT_EL	0x00002000	/* External Loopback Frame */
#define	TDSTAT_Rx_DT_r	0x00003000	/* Reserved */

/*
 * Descriptor Status bits for Transmit Descriptor.
 */
#define	TDSTAT_WINB_Tx_TE 0x00008000	/* Transmit Error */
#define	TDSTAT_Tx_TO	0x00004000	/* Transmit Jabber Timeout */
#define	TDSTAT_Tx_LO	0x00000800	/* Loss of Carrier */
#define	TDSTAT_Tx_NC	0x00000400	/* No Carrier */
#define	TDSTAT_Tx_LC	0x00000200	/* Late Collision */
#define	TDSTAT_Tx_EC	0x00000100	/* Excessive Collisions */
#define	TDSTAT_Tx_HF	0x00000080	/* Heartbeat Fail */
#define	TDSTAT_Tx_CC	0x00000078	/* Collision Count */
#define	TDSTAT_Tx_LF	0x00000004	/* Link Fail */
#define	TDSTAT_Tx_UF	0x00000002	/* Underflow Error */
#define	TDSTAT_Tx_DE	0x00000001	/* Deferred */

#define	TDSTAT_Tx_COLLISIONS(x)	(((x) & TDSTAT_Tx_CC) >> 3)

/*
 * Descriptor Control bits common to transmit and receive.
 */
#define	TDCTL_SIZE1	0x000007ff	/* Size of buffer 1 */
#define	TDCTL_SIZE1_SHIFT 0

#define	TDCTL_SIZE2	0x003ff800	/* Size of buffer 2 */
#define	TDCTL_SIZE2_SHIFT 11

#define	TDCTL_ER	0x02000000	/* End of Ring */
#define	TDCTL_CH	0x01000000	/* Second Address Chained */

/*
 * Descriptor Control bits for Transmit Descriptor.
 */
#define	TDCTL_Tx_IC	0x80000000	/* Interrupt on Completion */
#define	TDCTL_Tx_LS	0x40000000	/* Last Segment */
#define	TDCTL_Tx_FS	0x20000000	/* First Segment */
#define	TDCTL_Tx_FT1	0x10000000	/* Filtering Type 1 */
#define	TDCTL_Tx_SET	0x08000000	/* Setup Packet */
#define	TDCTL_Tx_AC	0x04000000	/* Add CRC Disable */
#define	TDCTL_Tx_DPD	0x00800000	/* Disabled Padding */
#define	TDCTL_Tx_FT0	0x00400000	/* Filtering Type 0 */

/*
 * The Tulip filter is programmed by "transmitting" a Setup Packet
 * (indicated by TDCTL_Tx_SET).  The filtering type is indicated
 * as follows:
 *
 *	FT1	FT0	Description
 *	---	---	-----------
 *	0	0	Perfect Filtering: The Tulip interprets the
 *			descriptor buffer as a table of 16 MAC addresses
 *			that the Tulip should receive.
 *
 *	0	1	Hash Filtering: The Tulip interprets the
 *			descriptor buffer as a 512-bit hash table
 *			plus one perfect address.  If the incoming
 *			address is Multicast, the hash table filters
 *			the address, else the address is filtered by
 *			the perfect address.
 *
 *	1	0	Inverse Filtering: Like Perfect Filtering, except
 *			the table is addresses that the Tulip does NOT
 *			receive.
 *
 *	1	1	Hash-only Filtering: Like Hash Filtering, but
 *			physical addresses are matched by the hash table
 *			as well, and not by matching a single perfect
 *			address.
 *
 * A Setup Packet must always be 192 bytes long.  The Tulip can store
 * 16 MAC addresses.  If not all 16 are specified in Perfect Filtering
 * or Inverse Filtering mode, then unused entries should duplicate
 * one of the valid entries.
 */
#define	TDCTL_Tx_FT_PERFECT	0
#define	TDCTL_Tx_FT_HASH	TDCTL_Tx_FT0
#define	TDCTL_Tx_FT_INVERSE	TDCTL_Tx_FT1
#define	TDCTL_Tx_FT_HASHONLY	(TDCTL_Tx_FT1|TDCTL_Tx_FT0)

#define	TULIP_SETUP_PACKET_LEN	192
#define	TULIP_MAXADDRS		16
#define	TULIP_MCHASHSIZE	512
#define	TULIP_PNICII_HASHSIZE	128

/*
 * Maximum size of a Tulip Ethernet Address ROM or SROM.
 */
#define	TULIP_ROM_SIZE(bits)	(2 << (bits))
#define	TULIP_MAX_ROM_SIZE	512

/*
 * Format of the standard Tulip SROM information:
 *
 *	Byte offset	Size	Usage
 *	0		18	reserved
 *	18		1	SROM Format Version
 *	19		1	Chip Count
 *	20		6	IEEE Network Address
 *	26		1	Chip 0 Device Number
 *	27		2	Chip 0 Info Leaf Offset
 *	29		1	Chip 1 Device Number
 *	30		2	Chip 1 Info Leaf Offset
 *	32		1	Chip 2 Device Number
 *	33		2	Chip 2 Info Leaf Offset
 *	...		1	Chip n Device Number
 *	...		2	Chip n Info Leaf Offset
 *	...		...	...
 *	Chip Info Leaf Information
 *	...
 *	...
 *	...
 *	126		2	CRC32 checksum
 */
#define	TULIP_ROM_SROM_FORMAT_VERION		18		/* B */
#define	TULIP_ROM_CHIP_COUNT			19		/* B */
#define	TULIP_ROM_IEEE_NETWORK_ADDRESS		20
#define	TULIP_ROM_CHIPn_DEVICE_NUMBER(n)	(26 + ((n) * 3))/* B */
#define	TULIP_ROM_CHIPn_INFO_LEAF_OFFSET(n)	(27 + ((n) * 3))/* W */
#define	TULIP_ROM_CRC32_CHECKSUM		126		/* W */
#define	TULIP_ROM_CRC32_CHECKSUM1		94		/* W */

#define	TULIP_ROM_IL_SELECT_CONN_TYPE		0		/* W */
#define	TULIP_ROM_IL_MEDIA_COUNT		2		/* B */
#define	TULIP_ROM_IL_MEDIAn_BLOCK_BASE		3

#define	SELECT_CONN_TYPE_TP		0x0000
#define	SELECT_CONN_TYPE_BNC		0x0001
#define	SELECT_CONN_TYPE_AUI		0x0002
#define	SELECT_CONN_TYPE_100TX		0x0003
#define	SELECT_CONN_TYPE_100T4		0x0006
#define	SELECT_CONN_TYPE_100FX		0x0007
#define	SELECT_CONN_TYPE MII_10T	0x0009
#define	SELECT_CONN_TYPE_MII_100TX	0x000d
#define	SELECT_CONN_TYPE_MII_100T4	0x000f
#define	SELECT_CONN_TYPE_MII_100FX	0x0010
#define	SELECT_CONN_TYPE_TP_AUTONEG	0x0100
#define	SELECT_CONN_TYPE_TP_FDX		0x0204
#define	SELECT_CONN_TYPE_MII_10T_FDX	0x020a
#define	SELECT_CONN_TYPE_100TX_FDX	0x020e
#define	SELECT_CONN_TYPE_MII_100TX_FDX	0x0211
#define	SELECT_CONN_TYPE_TP_NOLINKPASS	0x0400
#define	SELECT_CONN_TYPE_ASENSE		0x0800
#define	SELECT_CONN_TYPE_ASENSE_POWERUP	0x8800
#define	SELECT_CONN_TYPE_ASENSE_AUTONEG	0x0900

#define	TULIP_ROM_MB_MEDIA_CODE		0x3f
#define	TULIP_ROM_MB_MEDIA_TP		0x00
#define	TULIP_ROM_MB_MEDIA_BNC		0x01
#define	TULIP_ROM_MB_MEDIA_AUI		0x02
#define	TULIP_ROM_MB_MEDIA_100TX	0x03
#define	TULIP_ROM_MB_MEDIA_TP_FDX	0x04
#define	TULIP_ROM_MB_MEDIA_100TX_FDX	0x05
#define	TULIP_ROM_MB_MEDIA_100T4	0x06
#define	TULIP_ROM_MB_MEDIA_100FX	0x07
#define	TULIP_ROM_MB_MEDIA_100FX_FDX	0x08

#define	TULIP_ROM_MB_EXT		0x40

#define	TULIP_ROM_MB_CSR13		1			/* W */
#define	TULIP_ROM_MB_CSR14		3			/* W */
#define	TULIP_ROM_MB_CSR15		5			/* W */

#define	TULIP_ROM_MB_SIZE(mc)		(((mc) & TULIP_ROM_MB_EXT) ? 7 : 1)

#define	TULIP_ROM_MB_NOINDICATOR	0x8000
#define	TULIP_ROM_MB_DEFAULT		0x4000
#define	TULIP_ROM_MB_POLARITY		0x0080
#define	TULIP_ROM_MB_OPMODE(x)		(((x) & 0x71) << 18)
#define	TULIP_ROM_MB_BITPOS(x)		(1 << (((x) & 0x0e) >> 1))

#define	TULIP_ROM_MB_21140_GPR		0	/* 21140[A] GPR block */
#define	TULIP_ROM_MB_21140_MII		1	/* 21140[A] MII block */
#define	TULIP_ROM_MB_21142_SIA		2	/* 2114[23] SIA block */
#define	TULIP_ROM_MB_21142_MII		3	/* 2114[23] MII block */
#define	TULIP_ROM_MB_21143_SYM		4	/* 21143 SYM block */
#define	TULIP_ROM_MB_21143_RESET	5	/* 21143 reset block */

#define	TULIP_ROM_GETW(data, off) ((uint32_t)(data)[(off)] |		\
				   (uint32_t)((data)[(off) + 1]) << 8)

/*
 * Tulip control registers.
 */

#define	TULIP_CSR0	0x00
#define	TULIP_CSR1	0x08
#define	TULIP_CSR2	0x10
#define	TULIP_CSR3	0x18
#define	TULIP_CSR4	0x20
#define	TULIP_CSR5	0x28
#define	TULIP_CSR6	0x30
#define	TULIP_CSR7	0x38
#define	TULIP_CSR8	0x40
#define	TULIP_CSR9	0x48
#define	TULIP_CSR10	0x50
#define	TULIP_CSR11	0x58
#define	TULIP_CSR12	0x60
#define	TULIP_CSR13	0x68
#define	TULIP_CSR14	0x70
#define	TULIP_CSR15	0x78
#define	TULIP_CSR16	0x80
#define	TULIP_CSR17	0x88
#define	TULIP_CSR18	0x90
#define	TULIP_CSR19	0x98
#define	TULIP_CSR20	0xa0
#define	TULIP_CSR21	0xa8
#define	TULIP_CSR22	0xb0
#define	TULIP_CSR23	0xb8
#define	TULIP_CSR24	0xc0
#define	TULIP_CSR25	0xc8
#define	TULIP_CSR26	0xd0
#define	TULIP_CSR27	0xd8
#define	TULIP_CSR28	0xe0
#define	TULIP_CSR29	0xe8
#define	TULIP_CSR30	0xf0
#define	TULIP_CSR31	0xf8

#define	TULIP_CSR_INDEX(csr)	((csr) >> 3)

/* CSR0 - Bus Mode */
#define	CSR_BUSMODE		TULIP_CSR0
#define	BUSMODE_SWR		0x00000001	/* software reset */
#define	BUSMODE_BAR		0x00000002	/* bus arbitration */
#define	BUSMODE_DSL		0x0000007c	/* descriptor skip length */
#define	BUSMODE_BLE		0x00000080	/* big endian */
						/* programmable burst length */
#define	BUSMODE_PBL_DEFAULT	0x00000000	/*     default value */
#define	BUSMODE_PBL_1LW		0x00000100	/*     1 longword */
#define	BUSMODE_PBL_2LW		0x00000200	/*     2 longwords */
#define	BUSMODE_PBL_4LW		0x00000400	/*     4 longwords */
#define	BUSMODE_PBL_8LW		0x00000800	/*     8 longwords */
#define	BUSMODE_PBL_16LW	0x00001000	/*    16 longwords */
#define	BUSMODE_PBL_32LW	0x00002000	/*    32 longwords */
						/* cache alignment */
#define	BUSMODE_CAL_NONE	0x00000000	/*     no alignment */
#define	BUSMODE_CAL_8LW		0x00004000	/*     8 longwords */
#define	BUSMODE_CAL_16LW	0x00008000	/*    16 longwords */
#define	BUSMODE_CAL_32LW	0x0000c000	/*    32 longwords */
#define	BUSMODE_DAS		0x00010000	/* diagnostic address space */
						/*   must be zero on most */
						/* transmit auto-poll */
		/*
		 * Transmit auto-polling not supported on:
		 *	Winbond 89C040F
		 *	Xircom X3201-3
		 *	Davicom DM9102 (buggy BUSMODE register)
		 *	ASIX AX88140
		 */
#define	BUSMODE_TAP_NONE	0x00000000	/*     no auto-polling */
#define	BUSMODE_TAP_200us	0x00020000	/*   200 uS */
#define	BUSMODE_TAP_800us	0x00040000	/*   400 uS */
#define	BUSMODE_TAP_1_6ms	0x00060000	/*   1.6 mS */
#define	BUSMODE_TAP_12_8us	0x00080000	/*  12.8 uS (21041+) */
#define	BUSMODE_TAP_25_6us	0x000a0000	/*  25.6 uS (21041+) */
#define	BUSMODE_TAP_51_2us	0x000c0000	/*  51.2 uS (21041+) */
#define	BUSMODE_TAP_102_4us	0x000e0000	/* 102.4 uS (21041+) */
#define	BUSMODE_DBO		0x00100000	/* desc-only b/e (21041+) */
#define	BUSMODE_RME		0x00200000	/* rd/mult enab (21140+) */
#define	BUSMODE_WINB_WAIT	0x00200000	/* wait state insertion */
#define	BUSMODE_RLE		0x00800000	/* rd/line enab (21140+) */
#define	BUSMODE_WLE		0x01000000	/* wt/line enab (21140+) */
#define	BUSMODE_PNIC_MBO	0x04000000	/* magic `must be one' bit */
						/*    on Lite-On PNIC */


/* CSR1 - Transmit Poll Demand */
#define	CSR_TXPOLL		TULIP_CSR1
#define	TXPOLL_TPD		0x00000001	/* transmit poll demand */


/* CSR2 - Receive Poll Demand */
#define	CSR_RXPOLL		TULIP_CSR2
#define	RXPOLL_RPD		0x00000001	/* receive poll demand */


/* CSR3 - Receive List Base Address */
#define	CSR_RXLIST		TULIP_CSR3

/* CSR4 - Transmit List Base Address */
#define	CSR_TXLIST		TULIP_CSR4

/* CSR5 - Status */
#define	CSR_STATUS		TULIP_CSR5
#define	STATUS_TI		0x00000001	/* transmit interrupt */
#define	STATUS_TPS		0x00000002	/* transmit process stopped */
#define	STATUS_TU		0x00000004	/* transmit buffer unavail */
#define	STATUS_TJT		0x00000008	/* transmit jabber timeout */
#define	STATUS_WINB_REI		0x00000008	/* receive early interrupt */
#define	STATUS_LNPANC		0x00000010	/* link pass (21041) */
#define	STATUS_WINB_RERR	0x00000010	/* receive error */
#define	STATUS_UNF		0x00000020	/* transmit underflow */
#define	STATUS_RI		0x00000040	/* receive interrupt */
#define	STATUS_RU		0x00000080	/* receive buffer unavail */
#define	STATUS_RPS		0x00000100	/* receive process stopped */
#define	STATUS_RWT		0x00000200	/* receive watchdog timeout */
#define	STATUS_AT		0x00000400	/* SIA AUI/TP pin changed
						   (21040) */
#define	STATUS_ETI		0x00000400	/* early transmit interrupt
						   (21142/PMAC/Winbond) */
#define	STATUS_FD		0x00000800	/* full duplex short frame
						   received (21040) */
#define	STATUS_TM		0x00000800	/* timer expired (21041) */
#define	STATUS_LNF		0x00001000	/* link fail (21040) */
#define	STATUS_SE		0x00002000	/* system error */
#define	STATUS_ER		0x00004000	/* early receive (21041) */
#define	STATUS_AIS		0x00008000	/* abnormal interrupt summary */
#define	STATUS_NIS		0x00010000	/* normal interrupt summary */
#define	STATUS_RS		0x000e0000	/* receive process state */
#define	STATUS_RS_STOPPED	0x00000000	/* Stopped */
#define	STATUS_RS_FETCH		0x00020000	/* Running - fetch receive
						   descriptor */
#define	STATUS_RS_CHECK		0x00040000	/* Running - check for end
						   of receive */
#define	STATUS_RS_WAIT		0x00060000	/* Running - wait for packet */
#define	STATUS_RS_SUSPENDED	0x00080000	/* Suspended */
#define	STATUS_RS_CLOSE		0x000a0000	/* Running - close receive
						   descriptor */
#define	STATUS_RS_FLUSH		0x000c0000	/* Running - flush current
						   frame from FIFO */
#define	STATUS_RS_QUEUE		0x000e0000	/* Running - queue current
						   frame from FIFO into
						   buffer */
#define	STATUS_DM_RS_STOPPED	0x00000000	/* Stopped */
#define	STATUS_DM_RS_FETCH	0x00020000	/* Running - fetch receive
						   descriptor */
#define	STATUS_DM_RS_WAIT	0x00040000	/* Running - wait for packet */
#define	STATUS_DM_RS_QUEUE	0x00060000	/* Running - queue current
						   frame from FIFO into
						   buffer */
#define	STATUS_DM_RS_CLOSE_OWN	0x00080000	/* Running - close receive
						   descriptor, clear own */
#define	STATUS_DM_RS_CLOSE_ST	0x000a0000	/* Running - close receive
						   descriptor, write status */
#define	STATUS_DM_RS_SUSPENDED	0x000c0000	/* Suspended */
#define	STATUS_DM_RS_FLUSH	0x000e0000	/* Running - flush current
						   frame from FIFO */
#define	STATUS_TS		0x00700000	/* transmit process state */
#define	STATUS_TS_STOPPED	0x00000000	/* Stopped */
#define	STATUS_TS_FETCH		0x00100000	/* Running - fetch transmit
						   descriptor */
#define	STATUS_TS_WAIT		0x00200000	/* Running - wait for end
						   of transmission */
#define	STATUS_TS_READING	0x00300000	/* Running - read buffer from
						   memory and queue into
						   FIFO */
#define	STATUS_TS_RESERVED	0x00400000	/* RESERVED */
#define	STATUS_TS_SETUP		0x00500000	/* Running - Setup packet */
#define	STATUS_TS_SUSPENDED	0x00600000	/* Suspended */
#define	STATUS_TS_CLOSE		0x00700000	/* Running - close transmit
						   descriptor */
#define	STATUS_DM_TS_STOPPED	0x00000000	/* Stopped */
#define	STATUS_DM_TS_FETCH	0x00100000	/* Running - fetch transmit
						   descriptor */
#define	STATUS_DM_TS_SETUP	0x00200000	/* Running - Setup packet */
#define	STATUS_DM_TS_READING	0x00300000	/* Running - read buffer from
						   memory and queue into
						   FIFO */
#define	STATUS_DM_TS_CLOSE_OWN	0x00400000	/* Running - close transmit
						   descriptor, clear own */
#define	STATUS_DM_TS_WAIT	0x00500000	/* Running - wait for end
						   of transmission */
#define	STATUS_DM_TS_CLOSE_ST	0x00600000	/* Running - close transmit
						   descriptor, write status */
#define	STATUS_DM_TS_SUSPENDED	0x00700000	/* Suspended */
#define	STATUS_EB		0x03800000	/* error bits */
#define	STATUS_EB_PARITY	0x00000000	/* parity errror */
#define	STATUS_EB_MABT		0x00800000	/* master abort */
#define	STATUS_EB_TABT		0x01000000	/* target abort */
#define	STATUS_GPPI		0x04000000	/* GPIO interrupt (21142) */
#define	STATUS_PNIC_TXABORT	0x04000000	/* transmit aborted */
#define	STATUS_LC		0x08000000	/* 100baseTX link change
						   (21142/PMAC) */
#define	STATUS_PMAC_WKUPI	0x10000000	/* wake up event */
#define	STATUS_X3201_PMEIS	0x10000000	/* power management event
						   interrupt summary */
#define	STATUS_X3201_SFIS	0x80000000	/* second function (Modem)
						   interrupt status */


/* CSR6 - Operation Mode */
#define	CSR_OPMODE		TULIP_CSR6
#define	OPMODE_HP		0x00000001	/* hash/perfect mode (ro) */
#define	OPMODE_SR		0x00000002	/* start receive */
#define	OPMODE_HO		0x00000004	/* hash only mode (ro) */
#define	OPMODE_PB		0x00000008	/* pass bad frames */
#define	OPMODE_WINB_APP		0x00000008	/* accept all physcal packet */
#define	OPMODE_IF		0x00000010	/* inverse filter mode (ro) */
#define	OPMODE_WINB_AMP		0x00000010	/* accept multicast packet */
#define	OPMODE_SB		0x00000020	/* start backoff counter */
#define	OPMODE_WINB_ABP		0x00000020	/* accept broadcast packet */
#define	OPMODE_PR		0x00000040	/* promiscuous mode */
#define	OPMODE_WINB_ARP		0x00000040	/* accept runt packet */
#define	OPMODE_PM		0x00000080	/* pass all multicast */
#define	OPMODE_WINB_AEP		0x00000080	/* accept error packet */
#define	OPMODE_FKD		0x00000100	/* flaky oscillator disable */
#define OPMODE_AX_RB		0x00000100	/* recieve broadcast packets */
#define	OPMODE_FD		0x00000200	/* full-duplex mode */
#define	OPMODE_OM		0x00000c00	/* operating mode */
#define	OPMODE_OM_NORMAL	0x00000000	/*     normal mode */
#define	OPMODE_OM_INTLOOP	0x00000400	/*     internal loopback */
#define	OPMODE_OM_EXTLOOP	0x00000800	/*     external loopback */
#define	OPMODE_FC		0x00001000	/* force collision */
#define	OPMODE_ST		0x00002000	/* start transmitter */
#define	OPMODE_TR		0x0000c000	/* threshold control */
#define	OPMODE_TR_72		0x00000000	/*     72 bytes */
#define	OPMODE_TR_96		0x00004000	/*     96 bytes */
#define	OPMODE_TR_128		0x00008000	/*    128 bytes */
#define	OPMODE_TR_160		0x0000c000	/*    160 bytes */
#define	OPMODE_WINB_TTH		0x001fc000	/* transmit threshold */
#define	OPMODE_WINB_TTH_SHIFT	14
#define	OPMODE_BP		0x00010000	/* backpressure enable */
#define	OPMODE_CA		0x00020000	/* capture effect enable */
#define	OPMODE_PNIC_TBEN	0x00020000	/* Tx backoff offset enable */
	/*
	 * On Davicom DM9102, OPMODE_PS and OPMODE_HBD must
	 * always be set.
	 */
#define	OPMODE_PS		0x00040000	/* port select:
						   1 = MII/SYM, 0 = SRL
						   (21140) */
#define	OPMODE_HBD		0x00080000	/* heartbeat disable:
						   set in MII/SYM 100mbps,
						   set according to PHY
						   in MII 10mbps mode
						   (21140) */
#define	OPMODE_PNIC_IT		0x00100000	/* immediate transmit */
#define	OPMODE_SF		0x00200000	/* store and forward mode
						   (21140) */
#define	OPMODE_WINB_REIT	0x1fe00000	/* receive eartly intr thresh */
#define	OPMODE_WINB_REIT_SHIFT	21
#define	OPMODE_TTM		0x00400000	/* Transmit Threshold Mode:
						   1 = 10mbps, 0 = 100mbps
						   (21140) */
#define	OPMODE_PCS		0x00800000	/* PCS function (21140) */
#define	OPMODE_SCR		0x01000000	/* scrambler mode (21140) */
#define	OPMODE_MBO		0x02000000	/* must be one (21140,
						   DM9102) */
#define	OPMODE_IDAMSB		0x04000000	/* ignore dest addr MSB
						   (21142) */
#define	OPMODE_PNIC_DRC		0x20000000	/* don't include CRC in Rx
						   frames (PNIC) */
#define	OPMODE_WINB_FES		0x20000000	/* fast ethernet select */
#define	OPMODE_RA		0x40000000	/* receive all (21140) */
#define	OPMODE_PNIC_EED		0x40000000	/* 1 == ext, 0 == int ENDEC
						   (PNIC) */
#define	OPMODE_WINB_TEIO	0x40000000	/* transmit early intr on */
#define	OPMODE_SC		0x80000000	/* special capture effect
						   enable (21041+) */
#define	OPMODE_WINB_REIO	0x80000000	/* receive early intr on */

/* Shorthand for media-related OPMODE bits */
#define	OPMODE_MEDIA_BITS	(OPMODE_FD|OPMODE_PS|OPMODE_TTM|OPMODE_PCS|OPMODE_SCR)

/* CSR7 - Interrupt Enable */
#define	CSR_INTEN		TULIP_CSR7
	/* See bits for CSR5 -- Status */


/* CSR8 - Missed Frames */
#define	CSR_MISSED		TULIP_CSR8
#define	MISSED_MFC		0x0000ffff	/* missed packet count */
#define	MISSED_MFO		0x00010000	/* missed packet count
						   overflowed */
#define	MISSED_FOC		0x0ffe0000	/* fifo overflow counter
						   (21140) */
#define	MISSED_OCO		0x10000000	/* overflow counter overflowed
						   (21140) */

#define	MISSED_GETMFC(x)	((x) & MISSED_MFC)
#define	MISSED_GETFOC(x)	(((x) & MISSED_FOC) >> 17)


/* CSR9 - MII, SROM, Boot ROM, Ethernet Address ROM register. */
#define	CSR_MIIROM		TULIP_CSR9
#define	MIIROM_DATA		0x000000ff	/* byte of data from
						   Ethernet Address ROM
						   (21040), byte of data
						   to/from Boot ROM (21041+) */
#define	MIIROM_SROMCS		0x00000001	/* SROM chip select */
#define	MIIROM_SROMSK		0x00000002	/* SROM clock */
#define	MIIROM_SROMDI		0x00000004	/* SROM data in (to) */
#define	MIIROM_SROMDO		0x00000008	/* SROM data out (from) */
#define	MIIROM_REG		0x00000400	/* external register select */
#define	MIIROM_SR		0x00000800	/* SROM select */
#define	MIIROM_BR		0x00001000	/* boot ROM select */
#define	MIIROM_WR		0x00002000	/* write to boot ROM */
#define	MIIROM_RD		0x00004000	/* read from boot ROM */
#define	MIIROM_MOD		0x00008000	/* mode select (ro) (21041) */
#define	MIIROM_MDC		0x00010000	/* MII clock */
#define	MIIROM_MDO		0x00020000	/* MII data out */
#define	MIIROM_MIIDIR		0x00040000	/* MII direction mode
						   1 = PHY in read,
						   0 = PHY in write */
#define	MIIROM_MDI		0x00080000	/* MII data in */
#define	MIIROM_DN		0x80000000	/* data not valid (21040) */

#define	MIIROM_PMAC_LED0SEL	0x10000000	/* 0 == LED0 activity (def)
						   1 == LED0 speed */
#define	MIIROM_PMAC_LED1SEL	0x20000000	/* 0 == LED1 link (def)
						   1 == LED1 link/act */
#define	MIIROM_PMAC_LED2SEL	0x40000000	/* 0 == LED2 speed (def)
						   1 == LED2 collision */
#define	MIIROM_PMAC_LED3SEL	0x80000000	/* 0 == LED3 receive (def)
						   1 == LED3 full duplex */

	/* SROM opcodes */
#define	TULIP_SROM_OPC_ERASE	0x04
#define	TULIP_SROM_OPC_WRITE	0x05
#define	TULIP_SROM_OPC_READ	0x06

	/* The Lite-On PNIC does this completely differently */
#define	PNIC_MIIROM_DATA	0x0000ffff	/* mask of data bits ??? */
#define	PNIC_MIIROM_BUSY	0x80000000	/* EEPROM is busy */


/* CSR10 - Boot ROM address register (21041+). */
#define	CSR_ROMADDR		TULIP_CSR10
#define	ROMADDR_MASK		0x000003ff	/* boot rom address */


/* CSR11 - General Purpose Timer (21041+). */
#define	CSR_GPT			TULIP_CSR11
#define	GPT_VALUE		0x0000ffff	/* timer value */
#define	GPT_CON			0x00010000	/* continuous mode */
	/* 21143-PD and 21143-TD Interrupt Mitigation bits */
#define	GPT_NRX			0x000e0000	/* number of Rx packets */
#define	GPT_RXT			0x00f00000	/* Rx timer */
#define	GPT_NTX			0x07000000	/* number of Tx packets */
#define	GPT_TXT			0x78000000	/* Tx timer */
#define	GPT_CYCLE		0x80000000	/* cycle size */


/* CSR12 - SIA Status Register. */
#define	CSR_SIASTAT		TULIP_CSR12
#define	SIASTAT_PAUI		0x00000001	/* pin AUI/TP indication
						   (21040) */
#define	SIASTAT_MRA		0x00000001	/* MII receive activity
						   (21142) */
#define	SIASTAT_NCR		0x00000002	/* network connection error */
#define	SIASTAT_LS100		0x00000002	/* 100baseT link status
						   0 == pass (21142) */
#define	SIASTAT_LKF		0x00000004	/* link fail status */
#define	SIASTAT_LS10		0x00000004	/* 10baseT link status
						   0 == pass (21142) */
#define	SIASTAT_APS		0x00000008	/* auto polarity status */
#define	SIASTAT_DSD		0x00000010	/* PLL self test done */
#define	SIASTAT_DSP		0x00000020	/* PLL self test pass */
#define	SIASTAT_DAZ		0x00000040	/* PLL all zero */
#define	SIASTAT_DAO		0x00000080	/* PLL all one */
#define	SIASTAT_SRA		0x00000100	/* selected port receive
						   activity (21041) */
#define	SIASTAT_ARA		0x00000100	/* AUI receive activity
						   (21142) */
#define	SIASTAT_NRA		0x00000200	/* non-selected port
						   receive activity (21041) */
#define	SIASTAT_TRA		0x00000200	/* 10base-T receive activity
						   (21142) */
#define	SIASTAT_NSN		0x00000400	/* non-stable NLPs detected
						   (21041) */
#define	SIASTAT_TRF		0x00000800	/* transmit remote fault
						   (21041) */
#define	SIASTAT_ANS		0x00007000	/* autonegotiation state
						   (21041) */
#define	SIASTAT_ANS_DIS		0x00000000	/*     disabled */
#define	SIASTAT_ANS_TXDIS	0x00001000	/*     transmit disabled */
#define	SIASTAT_ANS_START	0x00001000	/*     (MX98715AEC) */
#define	SIASTAT_ANS_ABD		0x00002000	/*     ability detect */
#define	SIASTAT_ANS_ACKD	0x00003000	/*     acknowledge detect */
#define	SIASTAT_ANS_ACKC	0x00004000	/*     complete acknowledge */
#define	SIASTAT_ANS_FLPGOOD	0x00005000	/*     FLP link good */
#define	SIASTAT_ANS_LINKCHECK	0x00006000	/*     link check */
#define	SIASTAT_LPN		0x00008000	/* link partner negotiable
						   (21041) */
#define	SIASTAT_LPC		0xffff0000	/* link partner code word */

#define	SIASTAT_GETLPC(x)	(((x) & SIASTAT_LPC) >> 16)


/* CSR13 - SIA Connectivity Register. */
#define	CSR_SIACONN		TULIP_CSR13
#define	SIACONN_SRL		0x00000001	/* SIA reset
						   (0 == reset) */
#define	SIACONN_PS		0x00000002	/* pin AUI/TP selection
						   (21040) */
#define	SIACONN_CAC		0x00000004	/* CSR autoconfiguration */
#define	SIACONN_AUI		0x00000008	/* select AUI (0 = TP) */
#define	SIACONN_EDP		0x00000010	/* SIA PLL external input
						   enable (21040) */
#define	SIACONN_ENI		0x00000020	/* encoder input multiplexer
						   (21040) */
#define	SIACONN_SIM		0x00000040	/* serial interface input
						   multiplexer (21040) */
#define	SIACONN_ASE		0x00000080	/* APLL start enable
						   (21040) */
#define	SIACONN_SEL		0x00000f00	/* external port output
						   multiplexer select
						   (21040) */
#define	SIACONN_IE		0x00001000	/* input enable (21040) */
#define	SIACONN_OE1_3		0x00002000	/* output enable 1, 3
						   (21040) */
#define	SIACONN_OE2_4		0x00004000	/* output enable 2, 4
						   (21040) */
#define	SIACONN_OE5_6_7		0x00008000	/* output enable 5, 6, 7
						   (21040) */
#define	SIACONN_SDM		0x0000ef00	/* SIA diagnostic mode;
						   always set to this value
						   for normal operation
						   (21041) */


/* CSR14 - SIA Transmit Receive Register. */
#define	CSR_SIATXRX		TULIP_CSR14
#define	SIATXRX_ECEN		0x00000001	/* encoder enable */
#define	SIATXRX_LBK		0x00000002	/* loopback enable */
#define	SIATXRX_DREN		0x00000004	/* driver enable */
#define	SIATXRX_LSE		0x00000008	/* link pulse send enable */
#define	SIATXRX_CPEN		0x00000030	/* compensation enable */
#define	SIATXRX_CPEN_DIS0	0x00000000	/*     disabled */
#define	SIATXRX_CPEN_DIS1	0x00000010	/*     disabled */
#define	SIATXRX_CPEN_HIGHPWR	0x00000020	/*     high power */
#define	SIATXRX_CPEN_NORMAL	0x00000030	/*     normal */
#define	SIATXRX_MBO		0x00000040	/* must be one (21041 pass 2) */
#define	SIATXRX_TH		0x00000040	/* 10baseT HDX enable (21142) */
#define	SIATXRX_ANE		0x00000080	/* autonegotiation enable
						   (21041/21142) */
#define	SIATXRX_RSQ		0x00000100	/* receive squelch enable */
#define	SIATXRX_CSQ		0x00000200	/* collision squelch enable */
#define	SIATXRX_CLD		0x00000400	/* collision detect enable */
#define	SIATXRX_SQE		0x00000800	/* signal quality generation
						   enable */
#define	SIATXRX_LTE		0x00001000	/* link test enable */
#define	SIATXRX_APE		0x00002000	/* auto-polarity enable */
#define	SIATXRX_SPP		0x00004000	/* set polarity plus */
#define	SIATXRX_TAS		0x00008000	/* 10base-T/AUI autosensing
						   enable (21041/21142) */
#define	SIATXRX_THX		0x00010000	/* 100baseTX-HDX (21142) */
#define	SIATXRX_TXF		0x00020000	/* 100baseTX-FDX (21142) */
#define	SIATXRX_T4		0x00040000	/* 100baseT4 (21142) */


/* CSR15 - SIA General Register. */
#define	CSR_SIAGEN		TULIP_CSR15
#define	SIAGEN_JBD		0x00000001	/* jabber disable */
#define	SIAGEN_HUJ		0x00000002	/* host unjab */
#define	SIAGEN_JCK		0x00000004	/* jabber clock */
#define	SIAGEN_ABM		0x00000008	/* BNC select (21041) */
#define	SIAGEN_RWD		0x00000010	/* receive watchdog disable */
#define	SIAGEN_RWR		0x00000020	/* receive watchdog release */
#define	SIAGEN_LE1		0x00000040	/* LED 1 enable (21041) */
#define	SIAGEN_LV1		0x00000080	/* LED 1 value (21041) */
#define	SIAGEN_TSCK		0x00000100	/* test clock */
#define	SIAGEN_FUSQ		0x00000200	/* force unsquelch */
#define	SIAGEN_FLF		0x00000400	/* force link fail */
#define	SIAGEN_LSD		0x00000800	/* LED stretch disable
						   (21041) */
#define	SIAGEN_LEE		0x00000800	/* Link extend enable (21142) */
#define	SIAGEN_DPST		0x00001000	/* PLL self-test start */
#define	SIAGEN_FRL		0x00002000	/* force receiver low */
#define	SIAGEN_LE2		0x00004000	/* LED 2 enable (21041) */
#define	SIAGEN_RMP		0x00004000	/* received magic packet
						   (21143) */
#define	SIAGEN_LV2		0x00008000	/* LED 2 value (21041) */
#define	SIAGEN_HCKR		0x00008000	/* hacker (21143) */
#define	SIAGEN_MD		0x000f0000	/* general purpose mode/data */
#define	SIAGEN_LGS0		0x00100000	/* LED/GEP 0 select */
#define	SIAGEN_LGS1		0x00200000	/* LED/GEP 1 select */
#define	SIAGEN_LGS2		0x00400000	/* LED/GEP 2 select */
#define	SIAGEN_LGS3		0x00800000	/* LED/GEP 3 select */
#define	SIAGEN_GEI0		0x01000000	/* GEP pin 0 intr enable */
#define	SIAGEN_GEI1		0x02000000	/* GEP pin 1 intr enable */
#define	SIAGEN_RME		0x04000000	/* receive match enable */
#define	SIAGEN_CWE		0x08000000	/* control write enable */
#define	SIAGEN_GI0		0x10000000	/* GEP pin 0 interrupt */
#define	SIAGEN_GI1		0x20000000	/* GEP pin 1 interrupt */
#define	SIAGEN_RMI		0x40000000	/* receive match interrupt */


/* CSR12 - General Purpose Port (21140+). */
#define	CSR_GPP			TULIP_CSR12
#define	GPP_MD			0x000000ff	/* general purpose mode/data */
#define	GPP_GPC			0x00000100	/* general purpose control */
#define	GPP_PNIC_GPD		0x0000000f	/* general purpose data */
#define	GPP_PNIC_GPC		0x000000f0	/* general purpose control */

#define	GPP_PNIC_IN(x)		(1 << (x))
#define	GPP_PNIC_OUT(x, on)	(((on) << (x)) | (1 << ((x) + 4)))

/*
 * The Lite-On PNIC manual recommends the following for the General Purpose
 * I/O pins:
 *
 *	0	Speed Relay		1 == 100mbps
 *	1	100mbps loopback	1 == loopback
 *	2	BNC DC-DC converter	1 == select BNC
 *	3	Link 100		1 == 100baseTX link status
 */
#define	GPP_PNIC_PIN_SPEED_RLY	0
#define	GPP_PNIC_PIN_100M_LPKB	1
#define	GPP_PNIC_PIN_BNC_XMER	2
#define	GPP_PNIC_PIN_LNK100X	3

/*
 * Definitions used for the SMC 9332DST (21140) board.
 */
#define GPP_SMC9332DST_PINS	0x3f	/* General Purpose Pin directions */
#define GPP_SMC9332DST_OK10	0x80	/* 10 Mb/sec Signal Detect gep<7> */
#define GPP_SMC9332DST_OK100	0x40	/* 100 Mb/sec Signal Detect gep<6> */
#define GPP_SMC9332DST_INIT	0x09	/* No loopback --- point-to-point */

/*
 * Definitions used for the Cogent EM1x0 (21140) board.
 */
#define GPP_COGENT_EM1x0_PINS	0x3f	/* General Purpose Pin directions */
#define GPP_COGENT_EM1x0_INIT	0x09	/* No loopback --- point-to-point */

/*
 * Digital EB140 21140 reference design.
 * MC68832 + ML6671 for 100Mb/s.  LXT901 for 10Mb/s.
 *
 * (From document EC-QD2SA-TE, figure 1-3.)
 */
#define	GPP_EB140_OUTPUTS	0x1f	/* these GPP pins are driven */
#define	GPP_EB140_MC68832_LB	0x01	/* 100Mb/s loopback disable 1 */
#define	GPP_EB140_ML6671_LB	0x02	/* 100Mb/s loopback disable 2 */
#define	GPP_EB140_LXT901_ILB	0x04	/* 10Mb/s internal LB enable */
#define	GPP_EB140_LXT901_ELB	0x08	/* 10Mb/s external LB disable */
#define	GPP_EB140_RESERVED	0x10	/* media switch relay on other boards */
#define	GPP_EB140_MC68836_SYNC	0x20	/* synced to 100Mb/s PHY */
#define	GPP_EB140_MC68836_LINK	0x40	/* 100Mb/s signal detect */
#define	GPP_EB140_LXT901_LINK	0x80	/* 10Mb/s link pass */

#define	GPP_EB140_INIT	(GPP_EB140_LXT901_ELB|GPP_EB140_ML6671_LB|GPP_EB140_MC68832_LB)

/*
 * Digital Semiconductor 21040 registers.
 */

/* CSR11 - Full Duplex Register */
#define	CSR_21040_FDX		TULIP_CSR11
#define	FDX21040_FDXACV		0x0000ffff	/* full duplex
						   autoconfiguration value */


/* SIA configuration for 10base-T (from the 21040 manual) */
#define	SIACONN_21040_10BASET	0x0000ef01
#define	SIATXRX_21040_10BASET	0x0000ffff
#define	SIAGEN_21040_10BASET	0x00000000


/* SIA configuration for 10base-T full-duplex (from the 21040 manual) */
#define	SIACONN_21040_10BASET_FDX 0x0000ef01
#define	SIATXRX_21040_10BASET_FDX 0x0000fffd
#define	SIAGEN_21040_10BASET_FDX  0x00000000


/* SIA configuration for 10base-5 (from the 21040 manual) */
#define	SIACONN_21040_AUI	0x0000ef09
#define	SIATXRX_21040_AUI	0x00000705
#define	SIAGEN_21040_AUI	0x00000006


/* SIA configuration for External SIA (from the 21040 manual) */
#define	SIACONN_21040_EXTSIA	0x00003041
#define	SIATXRX_21040_EXTSIA	0x00000000
#define	SIAGEN_21040_EXTSIA	0x00000006


/*
 * Digital Semiconductor 21041 registers.
 */

/* SIA configuration for 10base-T (from the 21041 manual) */
#define	SIACONN_21041_10BASET	0x0000ef01
#define	SIATXRX_21041_10BASET	0x0000ff3f
#define	SIAGEN_21041_10BASET	0x00000000

#define	SIACONN_21041P2_10BASET	SIACONN_21041_10BASET
#define	SIATXRX_21041P2_10BASET	0x0000ffff
#define	SIAGEN_21041P2_10BASET	SIAGEN_21041_10BASET


/* SIA configuration for 10base-T full-duplex (from the 21041 manual) */
#define	SIACONN_21041_10BASET_FDX   0x0000ef01
#define	SIATXRX_21041_10BASET_FDX   0x0000ff3d
#define	SIAGEN_21041_10BASET_FDX    0x00000000

#define	SIACONN_21041P2_10BASET_FDX SIACONN_21041_10BASET_FDX
#define	SIATXRX_21041P2_10BASET_FDX 0x0000ffff
#define	SIAGEN_21041P2_10BASET_FDX  SIAGEN_21041_10BASET_FDX


/* SIA configuration for 10base-5 (from the 21041 manual) */
#define	SIACONN_21041_AUI	0x0000ef09
#define	SIATXRX_21041_AUI	0x0000f73d
#define	SIAGEN_21041_AUI	0x0000000e

#define	SIACONN_21041P2_AUI	SIACONN_21041_AUI
#define	SIATXRX_21041P2_AUI	0x0000f7fd
#define	SIAGEN_21041P2_AUI	SIAGEN_21041_AUI


/* SIA configuration for 10base-2 (from the 21041 manual) */
#define	SIACONN_21041_BNC	0x0000ef09
#define	SIATXRX_21041_BNC	0x0000f73d
#define	SIAGEN_21041_BNC	0x00000006

#define	SIACONN_21041P2_BNC	SIACONN_21041_BNC
#define	SIATXRX_21041P2_BNC	0x0000f7fd
#define	SIAGEN_21041P2_BNC	SIAGEN_21041_BNC


/*
 * Digital Semiconductor 21142/21143 registers.
 */

/* SIA configuration for 10baseT (from the 21143 manual) */
#define	SIACONN_21142_10BASET	0x00000001
#define	SIATXRX_21142_10BASET	0x00007f3f
#define	SIAGEN_21142_10BASET	0x00000008


/* SIA configuration for 10baseT full-duplex (from the 21143 manual) */
#define	SIACONN_21142_10BASET_FDX   0x00000001
#define	SIATXRX_21142_10BASET_FDX   0x00007f3d
#define	SIAGEN_21142_10BASET_FDX    0x00000008


/* SIA configuration for 10base5 (from the 21143 manual) */
#define	SIACONN_21142_AUI	0x00000009
#define	SIATXRX_21142_AUI	0x00004705
#define	SIAGEN_21142_AUI	0x0000000e


/* SIA configuration for 10base2 (from the 21143 manual) */
#define	SIACONN_21142_BNC	0x00000009
#define	SIATXRX_21142_BNC	0x00004705
#define	SIAGEN_21142_BNC	0x00000006


/*
 * Lite-On 82C168/82C169 registers.
 */

/* ENDEC General Register */
#define	CSR_PNIC_ENDEC		0x78
#define	PNIC_ENDEC_JDIS		0x00000001	/* jabber disable */

/* SROM Power Register */
#define	CSR_PNIC_SROMPWR	0x90
#define	PNIC_SROMPWR_MRLE	0x00000001	/* Memory-Read-Line enable */
#define	PNIC_SROMPWR_CB		0x00000002	/* cache boundary alignment
						   burst type; 1 == burst to
						   boundary, 0 == single-cycle
						   to boundary */

/* SROM Control Register */
#define	CSR_PNIC_SROMCTL	0x98
#define	PNIC_SROMCTL_addr	0x0000003f	/* mask of address bits */
/* XXX THESE ARE WRONG ACCORDING TO THE MANUAL! */
#define	PNIC_SROMCTL_READ	0x00000600	/* read command */

/* MII Access Register */
#define	CSR_PNIC_MII		0xa0
#define	PNIC_MII_DATA		0x0000ffff	/* mask of data bits */
#define	PNIC_MII_REG		0x007c0000	/* register mask */
#define	PNIC_MII_REGSHIFT	18
#define	PNIC_MII_PHY		0x0f800000	/* phy mask */
#define	PNIC_MII_PHYSHIFT	23
#define	PNIC_MII_OPCODE		0x30000000	/* opcode mask */
#define	PNIC_MII_RESERVED	0x00020000	/* must be one/must be zero;
						   2 bits are described here */
#define	PNIC_MII_MBO		0x40000000	/* must be one */
#define	PNIC_MII_BUSY		0x80000000	/* MII is busy */

#define	PNIC_MII_WRITE		0x10000000	/* write PHY command */
#define	PNIC_MII_READ		0x20000000	/* read PHY command */

/* NWAY Register */
#define	CSR_PNIC_NWAY		0xb8
#define	PNIC_NWAY_RS		0x00000001	/* reset NWay block */
#define	PNIC_NWAY_PD		0x00000002	/* power down NWay block */
#define	PNIC_NWAY_BX		0x00000004	/* bypass transceiver */
#define	PNIC_NWAY_LC		0x00000008	/* AUI low current mode */
#define	PNIC_NWAY_UV		0x00000010	/* low squelch voltage */
#define	PNIC_NWAY_DX		0x00000020	/* disable TP pol. correction */
#define	PNIC_NWAY_TW		0x00000040	/* select TP (0 == AUI) */
#define	PNIC_NWAY_AF		0x00000080	/* AUI full/half step input
						   voltage */
#define	PNIC_NWAY_FD		0x00000100	/* full duplex mode */
#define	PNIC_NWAY_DL		0x00000200	/* disable link integrity
						   test */
#define	PNIC_NWAY_DM		0x00000400	/* disable AUI/TP autodetect */
#define	PNIC_NWAY_100		0x00000800	/* 1 == 100mbps, 0 == 10mbps */
#define	PNIC_NWAY_NW		0x00001000	/* enable NWay block */
#define	PNIC_NWAY_CAP10T	0x00002000	/* adv. 10baseT */
#define	PNIC_NWAY_CAP10TFDX	0x00004000	/* adv. 10baseT-FDX */
#define	PNIC_NWAY_CAP100TXFDX	0x00008000	/* adv. 100baseTX-FDX */
#define	PNIC_NWAY_CAP100TX	0x00010000	/* adv. 100baseTX */
#define	PNIC_NWAY_CAP100T4	0x00020000	/* adv. 100base-T4 */
#define	PNIC_NWAY_RN		0x02000000	/* re-negotiate enable */
#define	PNIC_NWAY_RF		0x04000000	/* remote fault detected */
#define	PNIC_NWAY_LPAR10T	0x08000000	/* link part. 10baseT */
#define	PNIC_NWAY_LPAR10TFDX	0x10000000	/* link part. 10baseT-FDX */
#define	PNIC_NWAY_LPAR100TXFDX	0x20000000	/* link part. 100baseTX-FDX */
#define	PNIC_NWAY_LPAR100TX	0x40000000	/* link part. 100baseTX */
#define	PNIC_NWAY_LPAR100T4	0x80000000	/* link part. 100base-T4 */
#define	PNIC_NWAY_LPAR_MASK	0xf8000000


/*
 * Macronix 98713, 98713A, 98715, 98715A, 98715AEC, 98725 and
 * Lite-On 82C115 registers.
 */

	/*
	 * Note, the MX98713 is very Tulip-like:
	 *
	 *	CSR12		General Purpose Port (like 21140)
	 *	CSR13		reserved
	 *	CSR14		reserved
	 *	CSR15		Watchdog Timer (like 21140)
	 *
	 * The Macronix CSR12, CSR13, CSR14, and CSR15 exist only
	 * on the MX98713A and higher.
	 */

/* CSR12 - 10base-T Status Port (similar to SIASTAT) */
	/* See SIASTAT 21142/21143 bits */
#define	CSR_PMAC_10TSTAT	   TULIP_CSR12
#define	PMAC_SIASTAT_MASK	(SIASTAT_LS100|SIASTAT_LS10|		\
				 SIASTAT_APS|SIASTAT_TRF|SIASTAT_ANS|	\
				 SIASTAT_LPN|SIASTAT_LPC)


/* CSR13 - NWAY Reset Register */
#define	CSR_PMAC_NWAYRESET	TULIP_CSR13
	/* See SIACONN 21142/21143 bits */
#define	PMAC_SIACONN_MASK	(SIACONN_SRL)
#define	PMAC_NWAYRESET_100TXRESET 0x00000002	/* 100base PMD reset */


/* CSR14 - 10base-T Control Port */
#define	CSR_PMAC_10TCTL		TULIP_CSR14
	/* See SIATXRX 21142/21143 bits */
#define	PMAC_SIATXRX_MASK	(SIATXRX_LBK|SIATXRX_DREN|SIATXRX_TH|	\
				 SIATXRX_ANE|SIATXRX_RSQ|SIATXRX_LTE|	\
				 SIATXRX_THX|SIATXRX_TXF|SIATXRX_T4)


/* CSR15 - Watchdog Timer Register */
	/* MX98713: see 21140 CSR15 */
	/* others: see SIAGEN 21142/21143 bits */
#define	PMAC_SIAGEN_MASK	(SIAGEN_JBD|SIAGEN_HUJ|SIAGEN_JCK|	\
				 SIAGEN_RWD|SIAGEN_RWR)


/* CSR16 - Test Operation Register (a.k.a. Magic Packet Register) */
#define	CSR_PMAC_TOR		TULIP_CSR16
#define	PMAC_TOR_98713		0x0F370000
#define	PMAC_TOR_98715		0x0B3C0000


/* CSR20 - NWAY Status */
#define	CSR_PMAC_NWAYSTAT	TULIP_CSR20
	/*
	 * Note: the MX98715A manual claims that EQTEST and PCITEST
	 * must be set to 1 by software for normal operation, but
	 * this does not appear to be necessary.  This is probably
	 * one of the things that frobbing the Test Operation Register
	 * does.
	 *
	 * MX98715AEC uses this register for Auto Compensation.
	 * CSR20<14> and CSR20<9> are called DS130 and DS120
	 */
#define	PMAC_NWAYSTAT_DS120	0x00000200	/* Auto-compensation circ */
#define	PMAC_NWAYSTAT_DS130	0x00004000	/* Auto-compensation circ */
#define	PMAC_NWAYSTAT_EQTEST	0x00001000	/* EQ test */
#define	PMAC_NWAYSTAT_PCITEST	0x00010000	/* PCI test */
#define	PMAC_NWAYSTAT_10TXH	0x08000000	/* 10t accepted */
#define	PMAC_NWAYSTAT_10TXF	0x10000000	/* 10t-fdx accepted */
#define	PMAC_NWAYSTAT_100TXH	0x20000000	/* 100tx accepted */
#define	PMAC_NWAYSTAT_100TXF	0x40000000	/* 100tx-fdx accepted */
#define	PMAC_NWAYSTAT_T4	0x80000000	/* 100t4 accepted */


/* CSR21 - Flow Control Register */
#define	CSR_PNICII_FLOWCTL	TULIP_CSR21
#define	PNICII_FLOWCTL_WKFCATEN	0x00000010	/* enable wake-up frame
						   catenation feature */
#define	PNICII_FLOWCTL_NFCE	0x00000020	/* accept flow control result
						   from NWay */
#define	PNICII_FLOWCTL_FCTH0	0x00000040	/* rx flow control thresh 0 */
#define	PNICII_FLOWCTL_FCTH1	0x00000080	/* rx flow control thresh 1 */
#define	PNICII_FLOWCTL_REJECTFC	0x00000100	/* abort rx flow control */
#define	PNICII_FLOWCTL_STOPTX	0x00000200	/* tx flow stopped */
#define	PNICII_FLOWCTL_RUFCEN	0x00000400	/* send flow control when
						   RU interrupt occurs */
#define	PNICII_FLOWCTL_RXFCEN	0x00000800	/* rx flow control enable */
#define	PNICII_FLOWCTL_TXFCEN	0x00001000	/* tx flow control enable */
#define	PNICII_FLOWCTL_RESTOP	0x00002000	/* restop mode */
#define	PNICII_FLOWCTL_RESTART	0x00004000	/* restart mode */
#define	PNICII_FLOWCTL_TEST	0x00008000	/* test flow control timer */
#define	PNICII_FLOWCTL_TMVAL	0xffff0000	/* timer value in flow
						   control frame */

#define	PNICII_FLOWCTL_TH_512	(PNICII_FLOWCTL_FCTH0|PNICII_FLOWCTL_FCTH1)
#define	PNICII_FLOWCTL_TH_256	(PNICII_FLOWCTL_FCTH1)
#define	PNICII_FLOWCTL_TH_128	(PNICII_FLOWCTL_FCTH0)
#define	PNICII_FLOWCTL_TH_OVFLW	(0)


/* CSR22 - MAC ID Byte 3-0 Register */
#define	CSR_PNICII_MACID0	TULIP_CSR22
#define	PNICII_MACID_1		0	/* shift */
#define	PNICII_MACID_0		8	/* shift */
#define	PNICII_MACID_3		16	/* shift */
#define	PNICII_MACID_2		24	/* shift */


/* CSR23 - Magic ID Byte 5,4/MACID Byte 5,4 Register */
#define	PNICII_MACID_5		0	/* shift */
#define	PNICII_MACID_4		8	/* shift */
#define	PNICII_MAGID_5		16	/* shift */
#define	PNICII_MAGIC_4		24	/* shift */


/* CSR24 - Magic ID Byte 3-0 Register */
#define	PNICII_MAGID_1		0	/* shift */
#define	PNICII_MAGID_0		8	/* shift */
#define	PNICII_MAGID_3		16	/* shift */
#define	PNICII_MAGID_2		24	/* shift */


/* CSR25 - CSR28 - Filter Byte Mask Registers */
#define	CSR_PNICII_MASK0	TULIP_CSR25

#define	CSR_PNICII_MASK1	TULIP_CSR26

#define	CSR_PNICII_MASK2	TULIP_CSR27

#define	CSR_PNICII_MASK3	TULIP_CSR28


/* CSR29 - Filter Offset Register */
#define	CSR_PNICII_FILOFF	TULIP_CSR29
#define	PNICII_FILOFF_PAT0	0x0000007f	/* pattern 0 offset */
#define	PNICII_FILOFF_EN0	0x00000080	/* enable pattern 0 */
#define	PNICII_FILOFF_PAT1	0x00007f00	/* pattern 1 offset */
#define	PNICII_FILOFF_EN1	0x00008000	/* enable pattern 1 */
#define	PNICII_FILOFF_PAT2	0x007f0000	/* pattern 2 offset */
#define	PNICII_FILOFF_EN2	0x00800000	/* enable pattern 2 */
#define	PNICII_FILOFF_PAT3	0x7f000000	/* pattern 3 offset */
#define	PNICII_FILOFF_EN3	0x80000000	/* enable pattern 3 */


/* CSR30 - Filter 1 and 0 CRC-16 Register */
#define	CSR_PNICII_FIL01	TULIP_CSR30
#define	PNICII_FIL01_CRC0	0x0000ffff	/* CRC-16 of pattern 0 */
#define	PNICII_FIL01_CRC1	0xffff0000	/* CRC-16 of pattern 1 */


/* CSR31 = Filter 3 and 2 CRC-16 Register */
#define	CSR_PNICII_FIL23	TULIP_CSR31
#define	PNICII_FIL23_CRC2	0x0000ffff	/* CRC-16 of pattern 2 */
#define	PNICII_FIL23_CRC3	0xffff0000	/* CRC-16 of pattern 3 */


/*
 * Winbond 89C840F registers.
 */

/* CSR12 - Current Receive Descriptor Register */
#define	CSR_WINB_CRDAR		TULIP_CSR12


/* CSR13 - Current Receive Buffer Register */
#define	CSR_WINB_CCRBAR		TULIP_CSR13


/* CSR14 - Multicast Address Register 0 */
#define	CSR_WINB_CMA0		TULIP_CSR14


/* CSR15 - Multicast Address Register 1 */
#define	CSR_WINB_CMA1		TULIP_CSR15


/* CSR16 - Physical Address Register 0 */
#define	CSR_WINB_CPA0		TULIP_CSR16


/* CSR17 - Physical Address Register 1 */
#define	CSR_WINB_CPA1		TULIP_CSR17


/* CSR18 - Boot ROM Size Register */
#define	CSR_WINB_CBRCR		TULIP_CSR18
#define	WINB_CBRCR_NONE		0x00000000	/* no boot rom */
			/*	0x00000001	   also no boot rom */
#define	WINB_CBRCR_8K		0x00000002	/* 8k */
#define	WINB_CBRCR_16K		0x00000003	/* 16k */
#define	WINB_CBRCR_32K		0x00000004	/* 32k */
#define	WINB_CBRCR_64K		0x00000005	/* 64k */
#define	WINB_CBRCR_128K		0x00000006	/* 128k */
#define	WINB_CBRCR_256K		0x00000007


/* CSR19 - Current Transmit Descriptor Register */
#define	CSR_WINB_CTDAR		TULIP_CSR19


/* CSR20 - Current Transmit Buffer Register */
#define	CSR_WINB_CTBAR		TULIP_CSR20


/*
 * ADMtek AL981 registers
 *
 * We define these as strict byte offsets into PCI space, since
 * not all of them have consistent access rules.
 */

/* CSR13 - Wake-up Control/Status Register */
#define	CSR_ADM_WCSR		0x68
#define	ADM_WCSR_LSC		0x00000001	/* link status changed */
#define	ADM_WCSR_MPR		0x00000002	/* magic packet received */
#define	ADM_WCSR_WFR		0x00000004	/* wake up frame received */
#define	ADM_WCSR_LSCE		0x00000100	/* link status changed en. */
#define	ADM_WCSR_MPRE		0x00000200	/* magic packet receive en. */
#define	ADM_WCSR_WFRE		0x00000400	/* wake up frame receive en. */
#define	ADM_WCSR_LINKON		0x00010000	/* link-on detect en. */
#define	ADM_WCSR_LINKOFF	0x00020000	/* link-off detect en. */
#define	ADM_WCSR_WP5E		0x02000000	/* wake up pat. 5 en. */
#define	ADM_WCSR_WP4E		0x04000000	/* wake up pat. 4 en. */
#define	ADM_WCSR_WP3E		0x08000000	/* wake up pat. 3 en. */
#define	ADM_WCSR_WP2E		0x10000000	/* wake up pat. 2 en. */
#define	ADM_WCSR_WP1E		0x20000000	/* wake up pat. 1 en. */
#define	ADM_WCSR_CRCT		0x40000000	/* CRC-16 type:
						   0 == 0000 initial
						   1 == ffff initial */


/* CSR14 - Wake-up Pattern Data Register */
#define	CSR_ADM_WPDR		0x70

	/*
	 * 25 consecutive longword writes are issued to WPDR to
	 * program the wake-up pattern filter.  The data written
	 * is as follows:
	 *
	 *	XXX
	 */


/* CSR15 - see 21140 CSR15 (Watchdog Timer) */


/* CSR16 - Assistant CSR5 (Status Register 2) */
#define	CSR_ADM_ASR		0x80
						/* 0 - 14: same as CSR5 */
#define	ADM_ASR_AAISS		0x00080000	/* added abnormal int. sum. */
#define	ADM_ASR_ANISS		0x00010000	/* added normal int. sum. */
						/* XXX Receive state */
						/* XXX Transmit state */
#define	ADM_ASR_BET		0x03800000	/* bus error type */
#define	ADM_ASR_BET_PERR	0x00000000	/*   parity error */
#define	ADM_ASR_BET_MABT	0x00800000	/*   master abort */
#define	ADM_ASR_BET_TABT	0x01000000	/*   target abort */
#define	ADM_ASR_PFR		0x04000000	/* PAUSE frame received */
#define	ADM_ASR_TDIS		0x10000000	/* transmit def. int. status */
#define	ADM_ASR_XIS		0x20000000	/* xcvr int. status */
#define	ADM_ASR_REIS		0x40000000	/* receive early int. status */
#define	ADM_ASR_TEIS		0x80000000	/* transmit early int. status */


/* CSR17 - Assistant CSR7 (Interrupt Enable Register 2) */
#define	CSR_ADM_AIE		0x84
	/* See CSR16 for valid bits */


/* CSR18 - Command Register */
#define	CSR_ADM_CR		0x88
#define	ADM_CR_ATUR		0x00000001	/* auto. tx underrun recover */
#define	ADM_CR_SINT		0x00000002	/* software interrupt */
#define	ADM_CR_DRT		0x0000000c	/* drain receive threshold */
#define	ADM_CR_DRT_8LW		0x00000000	/*   8 longwords */
#define	ADM_CR_DRT_16LW		0x00000004	/*   16 longwords */
#define	ADM_CR_DRT_SF		0x00000008	/*   store-and-forward */
#define	ADM_CR_RTE		0x00000010	/* receive threshold enable */
#define	ADM_CR_PAUSE		0x00000020	/* enable PAUSE function */
#define	ADM_CR_RWP		0x00000040	/* reset wake-up pattern
						   data register pointer */
	/* 16 - 31 are automatically recalled from the EEPROM */
#define	ADM_CR_WOL		0x00040000	/* wake-on-lan enable */
#define	ADM_CR_PM		0x00080000	/* power management enable */
#define	ADM_CR_RFS		0x00600000	/* Receive FIFO size */
#define	ADM_CR_RFS_1K		0x00600000	/*   1K FIFO */
#define	ADM_CR_RFS_2K		0x00400000	/*   2K FIFO */
#define	ADM_CR_LEDMODE		0x00800000	/* LED mode */
#define	ADM_CR_AUXCL		0x30000000	/* aux current load */
#define	ADM_CR_D3CS		0x80000000	/* D3 cold wake up enable */


/* CSR19 - PCI bus performance counter */
#define	CSR_ADM_PCIC		0x8c
#define	ADM_PCIC_DWCNT		0x000000ff	/* double-word count of
						   last bus-master
						   transaction */
#define	ADM_PCIC_CLKCNT		0xffff0000	/* number of PCI clocks
						   between read request
						   and access completed */

/* CSR20 - Power Management Control/Status Register */
#define	CSR_ADM_PMCSR		0x90
	/*
	 * This register is also mapped into the PCI configuration
	 * space as the PMCSR.
	 */


/* CSR23 - Transmit Burst Count/Time Out Register */
#define	CSR_ADM_TXBR		0x9c
#define	ADM_TXBR_TTO		0x00000fff	/* transmit timeout */
#define	ADM_TXBR_TBCNT		0x001f0000	/* transmit burst count */


/* CSR24 - Flash ROM Port Register */
#define	CSR_ADM_FROM		0xa0
#define	ADM_FROM_DATA		0x000000ff	/* data to/from Flash */
#define	ADM_FROM_ADDR		0x01ffff00	/* Flash address */
#define	ADM_FROM_ADDR_SHIFT	8
#define	ADM_FROM_WEN		0x04000000	/* write enable */
#define	ADM_FROM_REN		0x08000000	/* read enable */
#define	ADM_FROM_bra16on	0x80000000	/* pin 87 is brA16, else
						   pin 87 is fd/col LED pin */


/* CSR25 - Physical Address Register 0 */
#define	CSR_ADM_PAR0		0xa4


/* CSR26 - Physical Address Register 1 */
#define	CSR_ADM_PAR1		0xa8


/* CSR27 - Multicast Address Register 0 */
#define	CSR_ADM_MAR0		0xac


/* CSR28 - Multicast Address Register 1 */
#define	CSR_ADM_MAR1		0xb0


/* Internal PHY registers are mapped here (lower 16 bits valid) */

#define	CSR_ADM_BMCR		0xb4
#define	CSR_ADM_BMSR		0xb8
#define	CSR_ADM_PHYIDR1		0xbc
#define	CSR_ADM_PHYIDR2		0xc0
#define	CSR_ADM_ANAR		0xc4
#define	CSR_ADM_ANLPAR		0xc8
#define	CSR_ADM_ANER		0xcc

/* XCVR Mode Control Register */
#define	CSR_ADM_XMC		0xd0
#define	ADM_XMC_LD		0x00000800	/* long distance mode
						   (low squelch enable) */


/* XCVR Configuration Information and Interrupt Status Register */
#define	CSR_ADM_XCIIS		0xd4
#define	ADM_XCIIS_REF		0x0001		/* 64 error packets received */
#define	ADM_XCIIS_ANPR		0x0002		/* autoneg page received */
#define	ADM_XCIIS_PDF		0x0004		/* parallel detection fault */
#define	ADM_XCIIS_ANAR		0x0008		/* autoneg ACK */
#define	ADM_XCIIS_LS		0x0010		/* link status (1 == fail) */
#define	ADM_XCIIS_RFD		0x0020		/* remote fault */
#define	ADM_XCIIS_ANC		0x0040		/* autoneg completed */
#define	ADM_XCIIS_PAUSE		0x0080		/* PAUSE enabled */
#define	ADM_XCIIS_DUPLEX	0x0100		/* full duplex */
#define	ADM_XCIIS_SPEED		0x0200		/* 100Mb/s */


/* XCVR Interrupt Enable Register */
#define	CSR_ADM_XIE		0xd8
	/* Bits are as for XCIIS */


/* XCVR 100baseTX PHY Control/Status Register */
#define	CSR_ADM_100CTR		0xdc
#define	ADM_100CTR_DISCRM	0x0001		/* disable scrambler */
#define	ADM_100CTR_DISMLT	0x0002		/* disable MLT3 ENDEC */
#define	ADM_100CTR_CMODE	0x001c		/* current operating mode */
#define	ADM_100CTR_CMODE_AUTO	0x0000		/*   in autoneg */
#define	ADM_100CTR_CMODE_10	0x0004		/*   10baseT */
#define	ADM_100CTR_CMODE_100	0x0008		/*   100baseTX */
			/*	0x000c		     reserved */
			/*	0x0010		     reserved */
#define	ADM_100CTR_CMODE_10FD	0x0014		/*   10baseT-FDX */
#define	ADM_100CTR_CMODE_100FD	0x0018		/*   100baseTX-FDX */
#define	ADM_100CTR_CMODE_ISO	0x001c		/*   isolated */
#define	ADM_100CTR_ISOTX	0x0020		/* transmit isolation */
#define	ADM_100CTR_ENRZI	0x0080		/* enable NRZ <> NRZI conv. */
#define	ADM_100CTR_ENDCR	0x0100		/* enable DC restoration */
#define	ADM_100CTR_ENRLB	0x0200		/* enable remote loopback */
#define	ADM_100CTR_RXVPP	0x0800		/* peak Rx voltage:
						   0 == 1.0 VPP
						   1 == 1.4 VPP */
#define	ADM_100CTR_ANC		0x1000		/* autoneg completed */
#define	ADM_100CTR_DISRER	0x2000		/* disable Rx error counter */

/* Operation Mode Register (AN983) */
#define	CSR_ADM983_OPMODE	0xfc
#define	ADM983_OPMODE_SPEED	0x80000000	/* 1 == 100, 0 == 10 */
#define	ADM983_OPMODE_FD	0x40000000	/* 1 == fd, 0 == hd */
#define	ADM983_OPMODE_LINK	0x20000000	/* 1 == link, 0 == no link */
#define	ADM983_OPMODE_EERLOD	0x04000000	/* reload from EEPROM */
#define	ADM983_OPMODE_SingleChip 0x00000007	/* single-chip mode */
#define	ADM983_OPMODE_MacOnly	 0x00000004	/* MAC-only mode */

/*
 * Xircom X3201-3 registers
 */

/* Power Management Register */
#define	CSR_X3201_PMR		TULIP_CSR16
#define	X3201_PMR_EDINT		0x0000000f	/* energy detect interval */
#define	X3201_PMR_EDEN		0x00000100	/* energy detect enable */
#define	X3201_PMR_MPEN		0x00000200	/* magic packet enable */
#define	X3201_PMR_WOLEN		0x00000400	/* Wake On Lan enable */
#define	X3201_PMR_PMGP0EN	0x00001000	/* GP0 change enable */
#define	X3201_PMR_PMLCEN	0x00002000	/* link change enable */
#define	X3201_PMR_WOLTMEN	0x00008000	/* WOL template mem enable */
#define	X3201_PMR_EP		0x00010000	/* energy present */
#define	X3201_PMR_LP		0x00200000	/* link present */
#define	X3201_PMR_EDES		0x01000000	/* ED event status */
#define	X3201_PMR_MPES		0x02000000	/* MP event status */
#define	X3201_PMR_WOLES		0x04000000	/* WOL event status */
#define	X3201_PMR_WOLPS		0x08000000	/* WOL process status */
#define	X3201_PMR_GP0ES		0x10000000	/* GP0 event status */
#define	X3201_PMR_LCES		0x20000000	/* LC event status */

/*
 * Davicom DM9102 registers.
 */

/* PHY Status Register */
#define	CSR_DM_PHYSTAT		TULIP_CSR12
#define	DM_PHYSTAT_10		0x00000001	/* 10Mb/s */
#define	DM_PHYSTAT_100		0x00000002	/* 100Mb/s */
#define	DM_PHYSTAT_FDX		0x00000004	/* full-duplex */
#define	DM_PHYSTAT_LINK		0x00000008	/* link up */
#define	DM_PHYSTAT_RXLOCK	0x00000010	/* RX-lock */
#define	DM_PHYSTAT_SIGNAL	0x00000020	/* signal detection */
#define	DM_PHYSTAT_UTPSIG	0x00000040	/* UTP SIG */
#define	DM_PHYSTAT_GPED		0x00000080	/* general PHY reset control */
#define	DM_PHYSTAT_GEPC		0x00000100	/* GPED bits control */


/* Sample Frame Access Register */
#define	CSR_DM_SFAR		TULIP_CSR13


/* Sample Frame Data Register */
#define	CSR_DM_SFDR		TULIP_CSR14
	/* See 21143 SIAGEN register */

/*
 * ASIX AX88140A and AX88141 registers.
 */

/* CSR13 - Filtering Index */
#define CSR_AX_FILTIDX		TULIP_CSR13

/* CSR14 - Filtering data */
#define CSR_AX_FILTDATA		TULIP_CSR14

/* Filtering Index values */
#define AX_FILTIDX_PAR0		0x00000000
#define AX_FILTIDX_PAR1		0x00000001
#define AX_FILTIDX_MAR0		0x00000002
#define AX_FILTIDX_MAR1		0x00000003

#endif /* _DEV_IC_TULIPREG_H_ */
