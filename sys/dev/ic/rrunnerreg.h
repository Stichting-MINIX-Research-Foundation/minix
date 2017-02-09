/*	$NetBSD: rrunnerreg.h,v 1.9 2008/04/28 20:23:51 martin Exp $	*/

/*
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code contributed to The NetBSD Foundation by Kevin M. Lahey
 * of the Numerical Aerospace Simulation Facility, NASA Ames Research
 * Center.
 *
 * Partially based on a HIPPI driver written by Essential Communications
 * Corporation.  Thanks to Jason Thorpe, Matt Jacob, and Fred Templin
 * for invaluable advice and encouragement!
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
 * Description of RoadRunner registers and hardware constructs.
 *
 * We're trying to support version 1 AND version 2 of the RunCode.
 * The fields that changed for version 2 are prefixed with RR2_ instead
 * of RR_.  If version 1 disappears (it is currently deprecated),
 * we can remove compatibility, but it seems a shame to lose functionality
 * for no good reason.
 */

/* PCI registers */

#define RR_PCI_BIST	0x0c	/* Built-In Self Test */

/* General control registers */

#define RR_MISC_HOST_CTL  0x40	/* Misc. Host Control */
#define RR_MISC_LOCAL_CTL 0x44	/* Misc. Local Control */
#define RR_PROC_PC	  0x48	/* i960 program counter */
#define RR_PROC_BREAKPT   0x4c	/* set breakpoint on i960 */
#define RR_TIMER	  0x54	/* clock */
#define RR_TIMER_REF	  0x58	/* When this matches the TIMER, interrupt */
#define RR_PCI_STATE	  0x5c	/* misc configuration */
#define RR_MAIN_EVENT	  0x60	/* main event register for i960 & RoadRunner */
#define RR_WINDOW_BASE	  0x68	/* pointer to internal memory*/
#define RR_WINDOW_DATA	  0x6c	/* value of mem at WINDOW_BASE */
#define RR_RX_STATE	  0x70	/* HIPPI receiver state */
#define RR_TX_STATE	  0x74	/* HIPPI transmitter state */
#define RR_EXT_SER_DATA	  0x7c	/* controls hardware besides RR on board */

/* Host DMA registers */

#define RR_WRITE_HOST		0x80	/* 64-bit pointer to data on host */
#define RR_READ_HOST		0x90
#define RR_WRITE_LENGTH		0x9c	/* length of data to be moved */
#define RR_READ_LENGTH		0xac
#define RR_DMA_WRITE_STATE	0xa0	/* controls DMA */
#define RR_DMA_READ_STATE	0xb0
#define RR_WRITE_DST		0xa4	/* Internal destination of DMA */
#define RR_READ_DST		0xb4

/* RunCode registers */

#define RR_EVENT_CONSUMER	0x200	/* index of consumer in event ring */
#define RR_SEND_PRODUCER	0x218	/* index of producer in sender ring */
#define RR_SNAP_RECV_PRODUCER	0x21c	/* index of producer in SNAP ring */
#define RR_RECVS_PRODUCER	0x220	/* index of producer in recv rings */
#define RR_COMMAND_RING		0x240	/* set of 16 command ring elements */

#define RR_ULA			0x280	/* Universal LAN Address */
#define RR_RECV_RING_PTR	0x288	/* receive ring address */
#define RR_GEN_INFO_PTR		0x290	/* general info block address */
#define RR_MODE_AND_STATUS	0x298	/* operating mode and status */
#define RR_CONN_RETRY_COUNT	0x29c	/* when no campon, try count */
#define RR_CONN_RETRY_TIMER	0x2a0	/* clock ticks to delay retry */
#define RR_CONN_TIMEOUT		0x2a4	/* campon delay timeout */
#define RR_STATS_TIMER		0x2a8	/* clock ticks between stats copy */
#define RR_MAX_RECV_RINGS	0x2ac	/* max receive rings (RO) */
#define RR_INTERRUPT_TIMER	0x2b0	/* clock ticks between interrupts */
#define RR_TX_TIMEOUT		0x2b4   /* transmit data not moving timer */
#define RR_RX_TIMEOUT		0x2b8	/* receive data not moving timer */
#define RR_EVENT_PRODUCER	0x2bc	/* index of producer in event ring */
#define RR_TRACE_INDEX		0x2c0	/* RunCode trace pointer */
#define RR_RUNCODE_FAIL1	0x2c4	/* failure codes */
#define RR_RUNCODE_FAIL2	0x2c8
#define RR_FILTER_LA		0x2d0	/* internal debug, filtering */
#define RR_RUNCODE_VERSION	0x2d4	/* RunCode version data */

#define RR_RUNCODE_RECV_CONS	0x300	/* Runcode receive ring consumption */
#define RR_DRIVER_RECV_CONS	0x320	/* Driver receive ring consumption */

#define RR_MEMORY_WINDOW	0x800	/* Memory window */


/*
 * Event codes
 */

/* General events */

#define RR_EC_RUNCODE_UP	0x01
#define RR_EC_WATCHDOG		0x02
#define RR_EC_TRACE		0x03
#define RR_EC_STATS_RETRIEVED	0x04
#define RR_EC_INVALID_CMD	0x05
#define RR_EC_SET_CMD_CONSUMER	0x06
#define RR_EC_LINK_ON		0x07
#define RR_EC_LINK_OFF		0x08
#define RR2_EC_INTERNAL_ERROR	0x09
#define RR_EC_INTERNAL_ERROR	0x0a
#define RR2_EC_SOFTWARE_ERROR	0x0a
#define RR_EC_STATS_UPDATE	0x0b
#define RR_EC_REJECTING		0x0c

/* Send events */

#define RR_EC_SET_SND_CONSUMER	0x10
#define RR_EC_PACKET_SENT	0x11
#define RR_EC_SEND_RING_LOW	0x12
#define RR_EC_CONN_REJECT	0x13
#define RR_EC_CAMPON_TIMEOUT	0x14
#define RR_EC_CONN_TIMEOUT	0x15
#define RR_EC_DISCONN_ERR	0x16
#define RR_EC_INTERNAL_PARITY	0x17
#define RR_EC_TX_IDLE		0x18
#define RR_EC_SEND_LINK_OFF	0x19
#define RR_EC_BAD_SEND_RING	0x1a
#define RR_EC_BAD_SEND_BUF	0x1b
#define RR_EC_BAD_SEND_DESC	0x1c

/* Receive events */

#define RR_EC_RING_ENABLED	0x20
#define RR_EC_RING_ENABLE_ERR	0x21
#define RR_EC_RING_DISABLED	0x22
#define RR_EC_RECV_RING_LOW	0x23
#define RR_EC_RECV_RING_OUT	0x24
#define RR_EC_PACKET_DISCARDED	0x25
#define RR_EC_RECV_RING_FLUSH	0x26
#define RR_EC_RECV_ERROR_INFO	0x27
#define RR_EC_SET_RECV_CONSUMER	0x29
#define RR_EC_PACKET_RECVED	0x2a
#define RR_EC_PARITY_ERR	0x2b
#define RR_EC_LLRC_ERR		0x2c
#define RR_EC_IP_HDR_CKSUM_ERR	0x2d
#define RR_EC_DATA_CKSUM_ERR	0x2e
#define RR_EC_SHORT_BURST_ERR	0x2f
#define RR_EC_RECV_LINK_OFF	0x30
#define RR_EC_FLAG_SYNC_ERR	0x31
#define RR_EC_FRAME_ERR		0x32
#define RR_EC_RECV_IDLE		0x33
#define RR_EC_PKT_LENGTH_ERR	0x34
#define RR_EC_STATE_TRANS_ERR	0x35
#define RR_EC_NO_READY_PULSE	0x3c
#define RR_EC_BAD_RECV_BUF	0x36
#define RR_EC_BAD_RECV_DESC	0x37
#define RR_EC_BAD_RECV_RING	0x38
#define RR_EC_NO_RING_FOR_ULP	0x3a
#define RR_EC_OUT_OF_BUF	0x3b
#define RR_EC_UNIMPLEMENTED	0x40


/*
 * Command codes
 */

#define RR_CC_START_RUNCODE	0x01
#define RR_CC_UPDATE_STATS	0x02
#define RR_CC_DISCONN_SRC	0x03
#define RR_CC_DISCONN_DST	0x04
#define RR_CC_WATCHDOG		0x05
#define RR_CC_TRACE		0x06
#define RR_CC_SET_SEND_PRODUCER	0x07
#define RR_CC_SET_RECV_PRODUCER	0x08
#define RR_CC_DISABLE_RING	0x09
#define RR_CC_ENABLE_RING	0x0a
#define RR_CC_DISCARD_PKT	0x0b
#define RR_CC_FLUSH_RECV_RING	0x0c  /* unimplemented */
#define RR_CC_CONN_MGT		0x0d


/*
 * Masks for registers
 */

/* Misc Host Control */

#define RR_MH_INTERRUPT 0x001	/* interrupt state */
#define RR_MH_CLEAR_INT 0x002	/* clear interrupt */
#define RR_MH_NO_SWAP	0x004	/* disable normal endian swap to host */
#define RR_MH_HALT_PROC	0x010	/* set to halt processor, clear to start */
#define RR_MH_STEP	0x020	/* set to single step processor */
#define RR_MH_PROC_HALT	0x100	/* indicates processor has been halted */
#define RR_MH_BAD_INSTR 0x200	/* indicates invalid instruction executed */

#define RR_MH_REVISION_MASK	0xf0000000 /* mask to retrieve revision code */

/* Misc Local Control */

#define RR_LC_CLEAR_INT  0x0002	/* clear interrupt */
#define RR_LC_FAST_PROM  0x0008	/* use fast EEPROM access */
#define RR_LC_ADD_SRAM	 0x0100	/* > 1MB SRAM present */
#define RR_LC_ADD_HIPPI	 0x0200	/* double number of HIPPI descriptors */
#define RR_LC_PARITY_ON	 0x0400	/* enable local parity checking */
#define RR_LC_WRITE_PROM 0x1000	/* EEPROM write enable */

/* PCI State */

#define RR_PS_READ_MASK       0x1c
#define RR_PS_READ_SHIFT      2
#define RR_PS_READ_DISABLE    (0 << RR_PS_READ_SHIFT)
#define RR_PS_READ_4	      (1 << RR_PS_READ_SHIFT)
#define RR_PS_READ_16	      (2 << RR_PS_READ_SHIFT)
#define RR_PS_READ_32	      (3 << RR_PS_READ_SHIFT)
#define RR_PS_READ_64	      (4 << RR_PS_READ_SHIFT)
#define RR_PS_READ_128	      (5 << RR_PS_READ_SHIFT)
#define RR_PS_READ_256	      (6 << RR_PS_READ_SHIFT)
#define RR_PS_READ_1024	      (7 << RR_PS_READ_SHIFT)

#define RR_PS_WRITE_MASK      0xe0
#define RR_PS_WRITE_SHIFT     5
#define RR_PS_WRITE_DISABLE   (0 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_4	      (1 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_16	      (2 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_32	      (3 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_64	      (4 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_128	      (5 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_256	      (6 << RR_PS_WRITE_SHIFT)
#define RR_PS_WRITE_1024      (7 << RR_PS_WRITE_SHIFT)
#define RR_PS_MIN_DMA_MASK    0xff00
#define RR_PS_MIN_DMA_SHIFT   8

/* HIPPI Receive State */

#define RR_RS_ENABLE	  0x01	/* enable new connections */
#define RR_RS_RESET	  0x02	/* reset receive interface */
#define RR_RS_REJECT_NONE 0x00	/* don't ever reject connections */
#define RR_RS_REJECT_2K	  0x20	/* reject if only 2KB free */
#define RR_RS_REJECT_4K	  0x30  /* reject if only 4KB free */
#define RR_RS_REJECT_8K	  0x40  /* reject if only 8KB free */
#define RR_RS_REJECT_16K  0x50  /* reject if only 16KB free */
#define RR_RS_REJECT_32K  0x60  /* reject if only 32KB free */
#define RR_RS_REJECT_64K  0x70  /* reject if only 64KB free */

/* HIPPI Transmit State */

#define RR_TS_ENABLE	0x01	/* enable transmit state machine */
#define RR_TS_PERMANENT	0x02	/* this connection permanent while set */

/* External Serial Data */

/*
 * This controls hardware that is external to the RoadRunner.
 * Bits 0-15 are set on write, 16-31 are read on read.
 */

#define RR_ES_TDAV	0x004	/* transmit data available */
#define RR_ES_LED1	0x008	/* LED1 control */
#define RR_ES_LED2	0x010	/* LED2 control */
#define RR_ES_RX_PERM	0x020	/* set permanent receive connection */
#define RR_ES_LEDAUTO	0x040	/* clear to let LED1 and LED2 control LEDs */
#define RR_ES_LLB_ENA	0x080	/* local loopback enable */
#define RR_ES_TP_START	0x100	/* test points (bits 8-13) */

#define RR_ES_REGINT	0x10000	/* interrupt from SEEQ-8100 (Gig-E) */
#define RR_ES_MISC	0x20000	/* misc input */
#define RR_ES_RXSIGDET	0x40000	/* fiber optic RXSIGDET output */

/* DMA Read State */

#define RR_DR_RESET		0x001	/* set to reset read DMA */
#define RR_DR_ACTIVE		0x008	/* set to start DMA */
#define RR_DR_THRESHOLD_MASK	0x1f0	/* mask off threshold values */
#define RR_DR_THRESHOLD_SHIFT	4	/* shift to set threshold values */
#define RR_DR_THRESHOLD_MAX	16

/* DMA Write State */

#define RR_DW_RESET		0x001 /* set to reset write DMA */
#define RR_DW_CKSUM		0x004 /* set to enable checksum calc on DMA */
#define RR_DW_ACTIVE		0x008 /* set to start DMA */
#define RR_DW_THRESHOLD_MASK	0x1f0 /* mask off threshold values */
#define RR_DW_THRESHOLD_SHIFT	4     /* shift to set threshold values */
#define RR_DW_THRESHOLD_MAX	18


/* Operating Mode and Status */

#define RR_MS_LOOPBACK	  0x0001  /* loopback through the GLink hardware */
#define RR_MS_PH_MODE	  0x0002  /* set for PH, clear for FP */
#define RR_MS_LONG_PTRS	  0x0004  /* set indicates 64-bit pointers */
#define RR_MS_WORD_SWAP	  0x0008  /* set to swap words in 64-bit pointers */
#define RR_MS_WARNINGS	  0x0010  /* set to enable warning events */
#define RR_MS_ERR_TERM	  0x0020  /* set to terminate connection on error */
#define RR_MS_DIRECT	  0x0040  /* debug flag.  enable filterLA checks */
#define RR_MS_NO_WATCHDOG 0x0080  /* set to disable watchdog */
#define RR_MS_SWAP_DATA	  0x0100  /* set to byte swap data */
#define RR_MS_SWAP_CNTRL  0x0200  /* set to byte swap control structures */
#define RR_MS_ERR_HALT	  0x0400  /* set to halt NIC on RunCode error */
#define RR_MS_NO_RESTART  0x0800  /* set to prevent NIC restart after error */
#define RR_MS_TX_HALFDUP  0x1000  /* NIC does half-duplex transmit */
#define RR_MS_RX_HALFDUP  0x2000  /* NIC does half-duplex receive */
#define RR_MS_GIG_E	  0x4000  /* NIC does Gig-E instead of HIPPI */

#define RR_MS_FATAL_ERR	  0x4000000  /* fatal error on NIC */
#define RR_MS_EVENT_OVER  0x8000000  /* event ring overflow */

/* Options field (top half of high word of ULA in RunCode) */

#define RR_OP_GIGE	0x01	/* Support for Gig-E NIC */
#define RR_OP_TRACED	0x02	/* Runcode generates debug traces */
#define RR_OP_1MEG	0x04	/* Support for 1MB of SRAM */
#define RR_OP_CDI	0x08	/* Support for Character Device Interace */
#define RR_OP_MSDOS	0x10	/* For testing RunCode under MS-DOS!? */
#define RR_OP_COMEV	0x20	/* New v2 Command/Event interface */
#define RR_OP_LONG_TX	0x40	/* Long transmit descr */
#define RR_OP_LONG_RX	0x80	/* Long receive descr (set when not CDI) */

/*
 * EEPROM locations
 *
 * The EEPROM layout is a little weird.  There is a valid byte every
 * eight bytes.  Words are then smeared out over 32 bytes.
 * All addresses listed here are the actual starting addresses.
 * The programmer is responsible for assembling a word from each of the
 * bytes available.
 *
 * NB:  This is incomplete.  I just ran out of patience for entering values.
 */

#define RR_EE_OFFSET	0x80000000	/* offset to the start of EEPROM mem */
#define RR_EE_WORD_LEN	0x20		/* jump between words in the EEPROM */
#define RR_EE_BYTE_LEN	0x08		/* jump between bytes in the EEPROM */
#define RR_EE_MAX_LEN	8192		/* maximum number of words in EEPROM */
#define RR_EE_SEG_SIZE	 512		/* maximum size of a segment */

#define RR_EE_PROM_INIT	0x801ff00	/* jump here to start RunCode loader */

#define RR_EE_HEADER_FORMAT_MAGIC	1     /* version number we can handle*/

#define RR_EE_SRAM_SIZE		0x0040  /* SRAM size */
#define RR_EE_PHASE1_START	0x0060  /* target byte address in SRAM */
#define RR_EE_PHASE1_LEN	0x0080  /* length in words of phase 1 */
#define RR_EE_PHASE1_EE_START	0x00a0  /* address of phase 1 in EEPROM */

#define RR_EE_PCI_DEV_VEND	0x0100	/* PCI device/vendor */
#define RR_EE_PCI_REV_CLASS	0x0120	/* PCI revision/class */
#define RR_EE_PCI_LATENCY	0x0140	/* PCI latency timer */
#define RR_EE_PCI_BAR0		0x0160	/* PCI bar0 address */
#define RR_EE_PCI_COMM_STAT	0x0180	/* PCI command/status */
#define RR_EE_PCI_LAT_GNT	0x01a0	/* PCI max latency/ minimum grant */
#define RR_EE_PCI_CHECKSUM	0x01f0	/* PCI area checksum */

#define RR_EE_HEADER_FORMAT	0x0200	/* revision of header format
					   (should be 1) */
#define RR_EE_ULA_HI		0x0500	/* Universal LAN Address (ULA) */
#define RR_EE_ULA_LO		0x0520

#define RR_EE_RUNCODE_START	0x0a00  /* runcode start PC */
#define RR_EE_RUNCODE_VERSION	0x0a20	/* runcode revision number */
#define RR_EE_RUNCODE_DATE	0x0a40	/* runcode revision date */
#define RR_EE_RUNCODE_SEGMENTS	0x0a80  /* address of count of segments */

#define RR_EE_MODE_AND_STATUS	0x0e00	/* mode and status saved value */
#define RR_EE_CONN_RETRY_COUNT	0x0e20	/* connection retry count */
#define RR_EE_CONN_RETRY_TIMER	0x0e40	/* clock ticks to delay retry */
#define RR_EE_CONN_TIMEOUT	0x0e60	/* campon delay timeout */
#define RR_EE_STATS_TIMER	0x0e80	/* clock ticks between stats copy */
#define RR_EE_INTERRUPT_TIMER	0x0ea0	/* clock ticks between interrupts */
#define RR_EE_TX_TIMEOUT	0x0ec0  /* transmit data not moving timer */
#define RR_EE_RX_TIMEOUT	0x0ee0	/* receive data not moving timer */

#define RR_EE_PCI_STATE		0x0f00	/* misc PCI DMA config */
#define RR_EE_DMA_WRITE_STATE	0x0f20	/* dma write config */
#define RR_EE_DMA_READ_STATE	0x0f40	/* dma read config */
#define RR_EE_DRIVER_PARAM	0x0f60	/* driver-specific params (unused) */

#define RR_EE_HEADER_CHECKSUM	0x0fe0	/* checksum for manufacturing header
					   (0x200 - 0xfc0) */

#define RR_EE_PHASE2_START	0x1040	/* phase 2 start in SRAM */
#define RR_EE_PHASE2_LENGTH	0x1060	/* phase 2 length in words */
#define RR_EE_PHASE2_EE_START	0x1080	/* phase 2 EEPROM start */

/*
 * Event descriptor
 */

struct rr_event {
	u_int16_t	re_index;	/* merge?  Different event types? */
	u_int8_t	re_ring;
	u_int8_t	re_code;
	u_int32_t	re_timestamp;
};

/*
 * Command descriptor
 */

union rr_cmd {
	struct {
		u_int16_t	rc_index;
		u_int8_t	rc_ring;
		u_int8_t	rc_code;
	} b;
	u_int32_t l;
};

/*
 * Scatter/gather descriptor -- points to buffers to be DMA'ed in and
 * out of host space.
 */

struct rr_descr {
	u_int32_t	rd_reserved1;
	u_int32_t	rd_buffer_addr;
	u_int32_t	rd_reserved2;
	u_int16_t	rd_length;
	u_int8_t	rd_ring;
	u_int8_t	rd_control;
#define RR_CT_TX_IPCKSUM   0x04
#define RR_CT_PACKET_END   0x08
#define RR_CT_PACKET_START 0x10
#define RR_CT_INTERRUPT	   0x20
#define RR_CT_SHORT_BURST  0x40
#define RR_CT_SAME_IFIELD  0x80
};

/*
 * Long scatter/gather descriptor -- similar to above descriptor,
 * but closer to RR's Assist register layout, so that it can just
 * be DMA'ed into the NIC, and let loose.  Required by Runcode v. 2
 * and greater.
 */

struct rr2_descr {
	u_int32_t	rd_reserved1;
	u_int32_t	rd_buffer_addr;
	u_int32_t	rd_reserved2;
	u_int16_t	rd_length;
	u_int8_t	rd_reserved3;
	u_int8_t	rd_control;
	u_int32_t	rd_dma_state;
#define RR_DM_TX	0x00606   /* swap code for dma_state on tx */
#define RR_DM_RX	0x20606   /* swap code for dma_state on rx */
	u_int32_t	rd_reserved4;
	u_int32_t	rd_pkt_length;
	u_int32_t	rd_reserved5;
#define RR2_CT_INTERRUPT    0x02
#define RR2_CT_TX_IPCKSUM   0x04
#define RR2_CT_PACKET_END   0x08
#define RR2_CT_PACKET_START 0x10
#define RR2_CT_RING_OUT     0x20
#define RR2_CT_SHORT_BURST  0x40
#define RR2_CT_SAME_IFIELD  0x80
};

/*
 * Ring control structure -- points to array of buffer descriptors
 */

struct rr_ring_ctl {
	u_int32_t	rr_reserved1;
	u_int32_t	rr_ring_addr;	/* pointer to array of s/g descrs */
	u_int16_t	rr_entries;
	u_int8_t	rr_free_bufs;
#define RR_RR_DONT_COMPLAIN	0xff	/* disable free_bufs warning */
	u_int8_t	rr_entry_size;
	u_int16_t	rr_prod_index;
	u_int16_t	rr_mode;
	/* XXX:  Picture in docs is right, description is wrong */
#define RR_RR_CHARACTER	0x01	/* character mode interface */
#define RR_RR_SEPARATE	0x02	/* separate headers from data */
#define RR_RR_CHECKSUM	0x04	/* doing IP checksums (requires IP on) */
#define RR_RR_IP	0x08	/* receiving IP packets to this ring */
};

/* Statistics block, for now, undifferentiated. */

struct rr_stats {
	u_int32_t	rs_stats[128];
};

/*
 * General information block
 */

struct rr_gen_info {
	struct rr_stats		ri_stats;
	struct rr_ring_ctl	ri_event_ring_ctl;
	struct rr_ring_ctl	ri_cmd_ring_ctl;
	struct rr_ring_ctl	ri_send_ring_ctl;
	u_int8_t		ri_reserved1[464];
	u_int8_t		ri_nic_trace[3072];
};

/*
 * A few constants:
 */

#define RR_DMA_BOUNDARY	(64 * 1024)	/* can't cross 64K boundaries on DMA */
#define RR_DMA_MAX	65535		/* maximum that can be transferred in
					   one DMA operation */
#define RR_ULP_COUNT	256		/* number of possible ULPs */
#define RR_INIT_CMD	15		/* initial command index */

/* NB:  All of the ring sizes have to be powers of two */

#define RR_MAX_RECV_RING	32	/* maximum number of receive rings */
#define RR_MAX_DESCR		256	/* maximum number of possible
					   descriptors.  XXX:  increase
					   with caution, as this allocates
					   static space! */
#define RR_EVENT_RING_SIZE	128	/* why not go for it? */
#define RR_SEND_RING_SIZE	32	/* firmware restriction! */

#define RR_SNAP_RECV_RING_SIZE	32	/* seems to work */
#define RR_FP_RECV_RING_SIZE	32	/* seems to work */

#define RR2_SEND_RING_SIZE	16	/* firmware restriction! */
#define RR2_SNAP_RECV_RING_SIZE	16	/* firmware restriction! */

#define RR_MAX_SEND_RING_SIZE	max(RR_SEND_RING_SIZE, RR2_SEND_RING_SIZE)
#define RR_MAX_SNAP_RECV_RING_SIZE	\
		max(RR_SNAP_RECV_RING_SIZE, RR_SNAP_RECV_RING_SIZE)
