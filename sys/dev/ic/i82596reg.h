/* $NetBSD: i82596reg.h,v 1.4 2008/04/05 08:42:35 skrll Exp $ */

/*
 * Copyright (c) 2003 Jochen Kunz.
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
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* All definitions are for a Intel 82596 DX/SX / CA in linear 32 bit mode. */

#define IEE_SYSBUS_BE	0x80	/* == 1 32 bit pointers are big endian */
#define IEE_SYSBUS_INT	0x20	/* == 1 interrupt pin is active low */
#define IEE_SYSBUS_LOCK	0x10	/* == 1 lock function disabled */
#define IEE_SYSBUS_TRG	0x08	/* == 1 external triggering of bus throttle */
#define IEE_SYSBUS_M1	0x04	/* M1 == 0 && M0 == 0 82586 mode */
#define IEE_SYSBUS_M0	0x02	/* M1 == 0 && M0 == 1 32 bit segmented mode */
				/* M1 == 1 && M0 == 0 linear mode */
				/* M1 == 1 && M0 == 1 reserved */
#define IEE_SYSBUS_M	0x06	/* mode mask */
#define IEE_SYSBUS_82586	0x00	/* 82586 mode */
#define IEE_SYSBUS_32SEG	0x02	/* 32 bit segmented mode */
#define IEE_SYSBUS_LIEAR	0x04	/* linear mode */
#define IEE_SYSBUS_STD	0x40	/* must be 1 all times */

#define IEE_PORT_RESET	0x0	/* PORT command reset */
#define IEE_PORT_SEFTST	0x1	/* PORT command self test */
#define IEE_PORT_SCP	0x2	/* PORT command set SCP */
#define IEE_PORT_DUMP	0x3	/* PORT command dump aread pointer */

/* System Control Block Command word.*/
#define IEE_SCB_ACK_CX	0x8000	/* CU completed an Action */
#define IEE_SCB_ACK_FR	0x4000	/* RU received a frame */
#define IEE_SCB_ACK_CNA	0x2000	/* CU became not active */
#define IEE_SCB_ACK_RNR	0x1000	/* RU became not active */
#define IEE_SCB_ACK	0xf000	/* Acknowledge mask */

#define IEE_SCB_CUC_NOP	0x0000	/* NOP, does not affect state of unit */
#define IEE_SCB_CUC_EXE	0x0100	/* Start execution of CMD on CBL */
#define IEE_SCB_CUC_RES	0x0200	/* Resume operat. of CU after suspend */
#define IEE_SCB_CUC_SUS	0x0300	/* Suspend exec. of cmds on CBL */
#define IEE_SCB_CUC_ABR	0x0400	/* Abort current command */
#define IEE_SCB_CUC_BT	0x0500	/* Load Bus Throttle */
#define IEE_SCB_CUC_BTI	0x0600	/* Load Bus Throttle immediately */
#define IEE_SCB_CUC	0x0700	/* Command mask */

#define IEE_SCB_RESET	0x0080	/* Reset the Chip */

#define IEE_SCB_RUC_NOP	0x0000	/* NOP, does not affect state of unit */
#define IEE_SCB_RUC_ST	0x0010	/* Start reception of frames */
#define IEE_SCB_RUC_RES	0x0020	/* Resume operat. of RU after suspend */
#define IEE_SCB_RUC_SUS	0x0030	/* Suspend frame reception */
#define IEE_SCB_RUC_ABR	0x0040	/* Abort receiver operat. immediately */
#define IEE_SCB_RUC	0x0070	/* Command mask */

/* System Control Block Status word.*/
#define IEE_SCB_STAT_CX	0x8000	/* CU finished cmd with int bit set */
#define IEE_SCB_STAT_FR	0x4000	/* RU finished receiving a frame */
#define IEE_SCB_STAT_CNA	0x2000	/* CU left active state */
#define IEE_SCB_STAT_RNR	0x1000	/* RU left ready state */
#define IEE_SCB_STAT		0xf000	/* Status mask */

#define IEE_SCB_CUS_IDL	0x0000	/* Idle */
#define IEE_SCB_CUS_SUS	0x0100	/* Suspend */
#define IEE_SCB_CUS_ACT	0x0200	/* Active */
#define IEE_SCB_CUS	0x0700	/* CU status bit mask */

#define IEE_SCB_RUS_IDL	0x0000	/* Idle */
#define IEE_SCB_RUS_SUS	0x0010	/* Suspend */
#define IEE_SCB_RUS_NR1	0x0020	/* No Resources (RFDs and / or RBDs) */
#define IEE_SCB_RUS_RDY	0x0040	/* Ready */
#define IEE_SCB_RUS_NR2	0x00a0	/* No Resources (no RBDs) */
#define IEE_SCB_RUS_NR3	0x00c0	/* No more RBDs */
#define IEE_SCB_RUS	0x00f0	/* RU status bit mask */

#define IEE_SCB_T	0x0008	/* Bus Throttle timers loaded */

#define IEE_SCB_TON	0x0000ffff	/* Bus Throttle TON mask */
#define IEE_SCB_TOFF	0xffff0000	/* Bus Throttle TOFF mask */

/* Bits in the Command Block Command word. */
#define IEE_CB_EL	0x8000	/* End of List, cmd is last on CBL */
#define IEE_CB_S	0x4000	/* Suspend after exec of this CB */
#define IEE_CB_I	0x2000	/* generate Interrupt after exec */
#define IEE_CB_NC	0x0010	/* No CRC insertion disable */
#define IEE_CB_SF	0x0008	/* Flexible Mode, data in TCB and TBD */

/* Bits in the Command Block Status word. */
#define IEE_CB_C	0x8000	/* Command is executed */
#define IEE_CB_B	0x4000	/* Command running or fetching CB */
#define IEE_CB_OK	0x2000	/* Command finished without error */
#define IEE_CB_A	0x1000	/* CU Abort control cmd was issued */
#define IEE_CB_F	0x0800	/* self test failed */
#define IEE_CB_EOF	0x8000	/* End Of Frame */
#define IEE_CB_STAT	0xf800	/* Status bit mask */
#define IEE_CB_COL	0x0020	/* TX stopped because of to much collisions */
#define IEE_CB_MAXCOL	0x000f	/* Number of Collisions mask */
/* Commands */
#define IEE_CB_CMD_NOP	0x0000	/* NOP */
#define IEE_CB_CMD_IAS	0x0001	/* Individual Address Setup */
#define IEE_CB_CMD_CONF	0x0002	/* Configure */
#define IEE_CB_CMD_MCS	0x0003	/* Multicast Setup */
#define IEE_CB_CMD_TR	0x0004	/* Transmit */
#define IEE_CB_CMD_TDR	0x0005	/* Time Domain Reflectometry */
#define IEE_CB_CMD_DUMP	0x0006	/* Dump */
#define IEE_CB_CMD_DIAG	0x0007	/* Diagnose */
#define IEE_CB_CMD	0x0007	/* CMD bit mask */

/* Receive Frame Descriptor bits */
#define IEE_RFD_EL	0x8000	/* End of List, RFD is last on list */
#define IEE_RFD_S	0x4000	/* Suspend after this RFD is filled */
#define IEE_RFD_SF	0x0008	/* Flexible Mode, data in RFD and RBD */
#define IEE_RFD_C	0x8000	/* Frame reception has completed */
#define IEE_RFD_B	0x4000	/* i82596 is busy on this RFD */
#define IEE_RFD_OK	0x2000	/* Frame received without error */
#define IEE_RFD_STAT	0x1fff	/* Status bits */
#define IEE_RFD_STAT_LEN	0x1000	/* Length error */
#define IEE_RFD_STAT_CRC	0x0800	/* CRC error */
#define IEE_RFD_STAT_ALIGN	0x0400	/* Alignment error */
#define IEE_RFD_STAT_NORES	0x0200	/* Ran out of buffer space */
#define IEE_RFD_STAT_DMA	0x0100	/* DMA Overrun */
#define IEE_RFD_STAT_SHORT	0x0080	/* Frame to short */
#define IEE_RFD_STAT_NOEOP	0x0040	/* No EOP Flag */
#define IEE_RFD_STAT_TRUNC	0x0020	/* Frame was truncated */
#define IEE_RFD_STAT_IA		0x0002	/* Frame doesn't match Individ. Addr. */
#define IEE_RFD_STAT_COLL	0x0001	/* Receive Collision */
#define IEE_RFD_EOF	0x8000		/* this is last buffer on list */
#define IEE_RFD_F	0x4000		/* buffer has already been used */
#define IEE_RFD_COUNT	0xc000		/* count mask */

/* Receive Buffer Descriptor bits */
#define IEE_RBD_EOF	0x8000		/* last buffer related to frame */
#define IEE_RBD_F	0x4000		/* buffer has already been used */
#define IEE_RBD_EL	0x8000		/* this is last buffer on list */
#define IEE_RBD_P	0x4000		/* this buffer is already prefetched */
#define IEE_RBD_COUNT	0x3fff		/* count mask */

/* Bits in Configure Bytes */
#define IEE_CF_0_CNT(x)		((x) & 0x0f)	/* Count of CF Bytes */
#define IEE_CF_0_CNT_DEF	0x0e	/* 14 Bytes is the default */
#define IEE_CF_0_CNT_M		0x0f	/* Mask */
#define IEE_CF_0_PREF		0x80	/* Write Prefetched bit */
#define IEE_CF_0_DEF		0x0e	/* Configuration Byte 0 Default Value */

#define IEE_CF_1_FIFO(x)	((x) & 0x0f)	/* FIFO Limit */
#define IEE_CF_1_FIFO_DEF	0x08	/* FIFO Default Value */
#define IEE_CF_1_MON2		(((x) & 0x3) << 6)	/* Monitor Bits */
#define IEE_CF_1_MON_DEF	0xc0	/* Monitor Bits Default */
#define IEE_CF_1_DEF		0xc8	/* Configuration Byte 1 Default Value */

#define IEE_CF_2_SAVBF		0x02	/* Save Bad frames */
#define IEE_CF_2_RESUM		0x80	/* Resume next CB */
#define IEE_CF_2_DEF		0x40	/* Configuration Byte 2 Default Value */
#define IEE_CF_2_STD		0x40	/* Configuration Byte 2 Standard Val. */

#define IEE_CF_3_ADDRLEN(x)	((x) & 0x07)	/* Address Length */
#define IEE_CF_3_ADDRLEN_DEF	0x06	/* Address Length Default */
#define IEE_CF_3_NSAI		0x08	/* No Source Address Insertion */
#define IEE_CF_3_ALLOC		0x08	/* == AL_LOC */
#define IEE_CF_3_PREAMLEN(x)	(((x) & 0x3) << 4)	/* Preamble Length */
#define IEE_CF_3_PREAMLEN_DEF	0x20	/*  */
#define IEE_CF_3_LOOPBK(x)	(((x) & 0x3) << 6)	/* Loopback Mode */
#define IEE_CF_3_LOOPBK_DEF	0x00	/*  */
#define IEE_CF_3_DEF		0x26	/* Configuration Byte 3 Default Value */

#define IEE_CF_4_LINPRIO(x)	((x) & 0x07)	/* Linear Priority */
#define IEE_CF_4_LINPRIO_DEF	0x00	/* Linear Priority */
#define IEE_CF_4_EXPPRIO(x)	(((x) & 0x07) << 4)	/* Exponential Prio. */
#define IEE_CF_4_EXPPRIO_DEF	0x00	/* Exponential Prio. */
#define IEE_CF_4_BOFMETD	0x80	/* Exponential Backoff Method */
#define IEE_CF_4_DEF		0x00	/* Configuration Byte 4 Default Value */

#define IEE_CF_5_IFSP(x)	((x) & 0xff)	/* Inter Frame Spacing */
#define IEE_CF_5_IFSP_DEF	0x60	/*  */
#define IEE_CF_5_DEF		0x60	/* Configuration Byte 5 Default Value */

#define IEE_CF_6_SLOT_TL(x) 	((x) & 0xff)	/* Slot Time Low */
#define IEE_CF_6_SLOT_TL_DEF	0x00	/*  */
#define IEE_CF_6_DEF		0x00	/* Configuration Byte 6 Default Value */

#define IEE_CF_7_SLOT_TH(x) 	((x) & 0x0f)	/* Slot Time High */
#define IEE_CF_7_SLOT_TH_DEF	0x02	/*  */
#define IEE_CF_7_RETR(x)	(((x) & 0x0f) << 4)	/* Num Retrans Retry */
#define IEE_CF_7_RETR_DEF	0xf0	/*  */
#define IEE_CF_7_DEF		0xf2	/* Configuration Byte 7 Default Value */

#define IEE_CF_8_PRM		0x01	/* Promiscuous Mode */
#define IEE_CF_8_BCDIS		0x02	/* Bradcast Disable */
#define IEE_CF_8_MANCH		0x04	/* Manchester encoding */
#define IEE_CF_8_TONO		0x08	/* Transmit on no CRS */
#define IEE_CF_8_NOCRCINS	0x10	/* No CRC Insertion */
#define IEE_CF_8_CRC16		0x20	/* CRC16 */
#define IEE_CF_8_BITSTF		0x40	/* Bit Stuffing */
#define IEE_CF_8_PAD		0x80	/* Padding */
#define IEE_CF_8_DEF		0x00	/* Configuration Byte 8 Default Value */

#define IEE_CF_9_CRSF(x)	((x) & 0x07)	/* Carrier Sense Filter Len */
#define IEE_CF_9_CRSF_DEF	0x00	/*  */
#define IEE_CF_9_CRSSRC		0x08	/* Carrier Sense Source */
#define IEE_CF_9_CDTF(x)	(((x) & 0x07) << 4)/* Carrier Detect Filt Len */
#define IEE_CF_9_CDTF_DEF	0x00	/*  */
#define IEE_CF_9_CDTSRC		0x80	/* Carrier Detect Source */
#define IEE_CF_9_DEF		0x00	/* Configuration Byte 9 Default Value */

#define IEE_CF_10_MINFRMLEN(x)	((x) & 0xff)	/* Minimum Frame Length */
#define IEE_CF_10_DEF		0x40	/* Configuration Byte 10 Default Val. */

#define IEE_CF_11_PRECRS	0x01	/* Preamble until Carrier Sense */
#define IEE_CF_11_LNGFLD	0x02	/* Length field. Enable padding */
#define IEE_CF_11_CRCINM	0x04	/* Rx CRC appended to the frame in MEM*/
#define IEE_CF_11_AUTOTX	0x08	/* Auto Retransmit when Coll in Preamb*/
#define IEE_CF_11_CDBSAC	0x10	/* Coll Detect by source Addr Recogn */
#define IEE_CF_11_MCALL		0x20	/* Enable to receive all MC Frames */
#define IEE_CF_11_MON(x)	(((x) & 0x03) << 6) /* Receive Monitor Bits */
#define IEE_CF_11_MON_DEF	0xc0	/*  */
#define IEE_CF_11_DEF		0xff	/* Configuration Byte 11 Default Val. */

#define IEE_CF_12_FDX		0x40	/* Enable Full Duplex */
#define IEE_CF_12_DEF		0x00	/* Configuration Byte 12 Default Val. */

#define IEE_CF_13_MULTIA	0x40	/* Multiple Individual Address */
#define IEE_CF_13_DISBOF	0x80	/* Disable the Backoff Algorithm */
#define IEE_CF_13_DEF		0x3f	/* Configuration Byte 13 Default Val. */

