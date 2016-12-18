/*	$NetBSD: ahareg.h,v 1.15 2008/05/10 11:52:20 martin Exp $	*/

/*-
 * Copyright (c) 1997-1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

typedef u_int8_t physaddr[3];
typedef u_int8_t physlen[3];
#define	ltophys	_lto3b
#define	phystol	_3btol

/*
 * I/O port offsets
 */
#define	AHA_CTRL_PORT		0	/* control (wo) */
#define	AHA_STAT_PORT		0	/* status (ro) */
#define	AHA_CMD_PORT		1	/* command (wo) */
#define	AHA_DATA_PORT		1	/* data (ro) */
#define	AHA_INTR_PORT		2	/* interrupt status (ro) */

/*
 * AHA_CTRL bits
 */
#define AHA_CTRL_HRST		0x80	/* Hardware reset */
#define AHA_CTRL_SRST		0x40	/* Software reset */
#define AHA_CTRL_IRST		0x20	/* Interrupt reset */
#define AHA_CTRL_SCRST		0x10	/* SCSI bus reset */

/*
 * AHA_STAT bits
 */
#define AHA_STAT_STST		0x80	/* Self test in Progress */
#define AHA_STAT_DIAGF		0x40	/* Diagnostic Failure */
#define AHA_STAT_INIT		0x20	/* Mbx Init required */
#define AHA_STAT_IDLE		0x10	/* Host Adapter Idle */
#define AHA_STAT_CDF		0x08	/* cmd/data out port full */
#define AHA_STAT_DF		0x04	/* Data in port full */
#define AHA_STAT_RSVD		0x02	/* Unused */
#define AHA_STAT_INVDCMD	0x01	/* Invalid command */

/*
 * AHA_CMD opcodes
 */
#define	AHA_NOP			0x00	/* No operation */
#define AHA_MBX_INIT		0x01	/* Mbx initialization */
#define AHA_START_SCSI		0x02	/* start scsi command */
#define AHA_INQUIRE_REVISION	0x04	/* Adapter Inquiry */
#define AHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#if 0
#define AHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define AHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define AHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define AHA_SPEED_SET		0x09	/* set transfer speed */
#endif
#define AHA_INQUIRE_DEVICES	0x0a	/* return installed devices 0-7 */
#define AHA_INQUIRE_CONFIG	0x0b	/* return configuration data */
#define AHA_TARGET_EN		0x0c	/* enable target mode */
#define AHA_INQUIRE_SETUP	0x0d	/* return setup data */
#define AHA_ECHO		0x1e	/* Echo command data */
#define AHA_INQUIRE_DEVICES_2	0x23	/* return installed devices 8-15 */
#define AHA_EXT_BIOS		0x28	/* return extended bios info */
#define AHA_MBX_ENABLE		0x29	/* enable mail box interface */

/*
 * AHA_INTR bits
 */
#define AHA_INTR_ANYINTR	0x80	/* Any interrupt */
#define AHA_INTR_RSVD		0x70	/* unused bits */
#define AHA_INTR_SCRD		0x08	/* SCSI reset detected */
#define AHA_INTR_HACC		0x04	/* Command complete */
#define AHA_INTR_MBOA		0x02	/* MBX out empty */
#define AHA_INTR_MBIF		0x01	/* MBX in full */

/*
 * AHA Board IDs
 */
#define BOARD_1540_16HEAD_BIOS  0x00
#define BOARD_1540_64HEAD_BIOS  0x30
#define BOARD_1540              0x31
#define BOARD_1542              0x41    /* aha-1540/1542 w/64-h bios */
#define BOARD_1640              0x42    /* aha-1640 */
#define BOARD_1740              0x43    /* aha-1740A/1742A/1744 */
#define BOARD_1542C             0x44    /* aha-1542C */
#define BOARD_1542CF            0x45    /* aha-1542CF */
#define BOARD_1542CP            0x46    /* aha-1542CP, plug and play */

struct aha_mbx_out {
	u_char cmd;
	physaddr ccb_addr;
};

struct aha_mbx_in {
	u_char stat;
	physaddr ccb_addr;
};

/*
 * mbo.cmd values
 */
#define AHA_MBO_FREE	0x0	/* MBO entry is free */
#define AHA_MBO_START	0x1	/* MBO activate entry */
#define AHA_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.stat values
 */
#define AHA_MBI_FREE	0x0	/* MBI entry is free */
#define AHA_MBI_OK	0x1	/* completed without error */
#define AHA_MBI_ABORT	0x2	/* aborted ccb */
#define AHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define AHA_MBI_ERROR	0x4	/* Completed with error */

/* FOR OLD VERSIONS OF THE !%$@ this may have to be 16 (yuk) */
#define	AHA_NSEG	17	/* Number of scatter gather segments <= 16 */
				/* allow 64 K i/o (min) */

struct aha_scat_gath {
	physlen seg_len;
	physaddr seg_addr;
};

struct aha_ccb {
	u_char opcode;
	u_char lun:3;
	u_char data_in:1;	/* must be 0 */
	u_char data_out:1;	/* must be 0 */
	u_char target:3;
	u_char scsi_cmd_length;
	u_char req_sense_length;
	physlen data_length;
	physaddr data_addr;
	physaddr link_addr;
	u_char link_id;
	u_char host_stat;
	u_char target_stat;
	u_char reserved[2];
	u_char scsi_cmd[12];
	struct scsi_sense_data scsi_sense;
	struct aha_scat_gath scat_gath[AHA_NSEG];
	/*----------------------------------------------------------------*/
	TAILQ_ENTRY(aha_ccb) chain;
	struct aha_ccb *nexthash;
	u_long hashkey;
	struct scsipi_xfer *xs;		/* the scsipi_xfer for this cmd */
	int flags;
#define	CCB_ALLOC	0x01
#define	CCB_ABORT	0x02
#ifdef AHADIAG
#define	CCB_SENDING	0x04
#endif
	int timeout;

	/*
	 * This DMA map maps the buffer involved in the transfer.
	 * Its contents are loaded into "scat_gath" above.
	 */
	bus_dmamap_t	dmamap_xfer;
};

/*
 * opcode fields
 */
#define AHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define AHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define AHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with scatter gather */
#define AHA_RESET_CCB		0x81	/* SCSI Bus reset */

/*
 * aha_ccb.host_stat values
 */
#define AHA_OK		0x00	/* cmd ok */
#define AHA_LINK_OK	0x0a	/* Link cmd ok */
#define AHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define AHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define AHA_OVER_UNDER	0x12	/* Data over/under run */
#define AHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define AHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define AHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define AHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define AHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define AHA_INV_TARGET	0x18	/* Invalid target direction */
#define AHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define AHA_INV_CCB	0x1a	/* Invalid CCB or segment list */

struct aha_revision {
	struct {
		u_char	opcode;
	} cmd;
	struct {
		u_char	boardid;	/* type of board */
					/* 0x31 = AHA-1540 */
					/* 0x41 = AHA-1540A/1542A/1542B */
					/* 0x42 = AHA-1640 */
					/* 0x43 = AHA-1542C */
					/* 0x44 = AHA-1542CF */
					/* 0x45 = AHA-1542CF, BIOS v2.01 */
					/* 0x46 = AHA-1542CP */
		u_char	spec_opts;	/* special options ID */
					/* 0x41 = Board is standard model */
		u_char	revision_1;	/* firmware revision [0-9A-Z] */
		u_char	revision_2;	/* firmware revision [0-9A-Z] */
	} reply;
};

struct aha_extbios {
	struct {
		u_char	opcode;
	} cmd;
	struct {
		u_char	flags;		/* Bit 3 == 1 extended bios enabled */
		u_char	mailboxlock;	/* mail box lock code to unlock it */
	} reply;
};

struct aha_toggle {
	struct {
		u_char	opcode;
		u_char	enable;
	} cmd;
};

struct aha_config {
	struct {
		u_char	opcode;
	} cmd;
	struct {
		u_char  chan;
		u_char  intr;
		u_char  scsi_dev:3;
		u_char	:5;
	} reply;
};

struct aha_mailbox {
	struct {
		u_char	opcode;
		u_char	nmbx;
		physaddr addr;
	} cmd;
};

struct aha_unlock {
	struct {
		u_char	opcode;
		u_char	junk;
		u_char	magic;
	} cmd;
};

struct aha_devices {
	struct {
		u_char	opcode;
	} cmd;
	struct {
		u_char	lun_map[8];
	} reply;
};

struct aha_setup {
	struct {
		u_char	opcode;
		u_char	len;
	} cmd;
	struct {
		u_char  sync_neg:1;
		u_char  parity:1;
		u_char	:6;
		u_char  speed;
		u_char  bus_on;
		u_char  bus_off;
		u_char  num_mbx;
		u_char  mbx[3];
		struct {
			u_char  offset:4;
			u_char  period:3;
			u_char  valid:1;
		} sync[8];
		u_char  disc_sts;
	} reply;
};

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define EISADMA	0x00
#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80
