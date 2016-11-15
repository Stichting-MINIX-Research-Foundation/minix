/*	$NetBSD: dptreg.h,v 1.19 2008/09/08 23:36:54 gmcgarry Exp $	*/

/*
 * Copyright (c) 1999, 2000, 2001 Andrew Doran <ad@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_IC_DPTREG_H_
#define	_IC_DPTREG_H_	1

/* Hardware limits */
#define DPT_MAX_TARGETS		16
#define DPT_MAX_LUNS		8
#define DPT_MAX_CHANNELS	3

/*
 * HBA registers
 */
#define HA_DATA			0
#define HA_ERROR		1
#define HA_DMA_BASE		2
#define HA_ICMD_CODE2	       	4
#define HA_ICMD_CODE1	       	5
#define HA_ICMD			6

/* EATA commands. There are many more that we don't define or use. */
#define HA_COMMAND		7
#define   CP_PIO_GETCFG		0xf0	/* Read configuration data, PIO */
#define   CP_PIO_CMD		0xf2	/* Execute command, PIO */
#define   CP_DMA_GETCFG		0xfd	/* Read configuration data, DMA */
#define   CP_DMA_CMD		0xff	/* Execute command, DMA */
#define   CP_PIO_TRUNCATE	0xf4	/* Truncate transfer command, PIO */
#define   CP_RESET		0xf9	/* Reset controller and SCSI bus */
#define   CP_REBOOT		0x06	/* Reboot controller (last resort) */
#define   CP_IMMEDIATE		0xfa	/* EATA immediate command */
#define     CPI_GEN_ABORT	0x00	/* Generic abort */
#define     CPI_SPEC_RESET	0x01	/* Specific reset */
#define     CPI_BUS_RESET	0x02	/* Bus reset */
#define     CPI_SPEC_ABORT	0x03	/* Specific abort */
#define     CPI_QUIET_INTR	0x04	/* ?? */
#define     CPI_ROM_DL_EN	0x05	/* ?? */
#define     CPI_COLD_BOOT	0x06	/* Cold boot HBA */
#define     CPI_FORCE_IO	0x07	/* ?? */
#define     CPI_BUS_OFFLINE	0x08	/* Set SCSI bus offline */
#define     CPI_RESET_MSKD_BUS	0x09	/* Reset masked bus */
#define     CPI_POWEROFF_WARN	0x0a	/* Power about to fail */

#define HA_STATUS		7
#define   HA_ST_ERROR		0x01
#define   HA_ST_MORE		0x02
#define   HA_ST_CORRECTD	0x04
#define   HA_ST_DRQ		0x08
#define   HA_ST_SEEK_COMPLETE	0x10
#define   HA_ST_WRT_FLT		0x20
#define   HA_ST_READY		0x40
#define   HA_ST_BUSY		0x80
#define   HA_ST_DATA_RDY	(HA_ST_SEEK_COMPLETE|HA_ST_READY|HA_ST_DRQ)

#define HA_AUX_STATUS		8
#define   HA_AUX_BUSY		0x01
#define   HA_AUX_INTR		0x02

/*
 * Structure of an EATA command packet.
 */
struct eata_cp {
	u_int8_t	cp_ctl0;		/* Control flags 0 */
	u_int8_t	cp_senselen;		/* Request sense length */
	u_int8_t	cp_unused0[3];		/* Unused */
	u_int8_t	cp_ctl1;		/* Control flags 1 */
	u_int8_t	cp_ctl2;		/* Control flags 2 */
	u_int8_t	cp_ctl3;		/* Control flags 3 */
	u_int8_t	cp_ctl4;		/* Control flags 4 */
	u_int8_t	cp_msg[3];		/* Message bytes 0-3 */
	u_int8_t	cp_cdb_cmd;		/* SCSI CDB */
	u_int8_t	cp_cdb_more0[3];	/* SCSI CDB */
	u_int8_t	cp_cdb_len;		/* SCSI CDB */
	u_int8_t	cp_cdb_more1[7];	/* SCSI CDB */

	u_int32_t	cp_datalen;		/* Bytes of data/SG list */
	u_int32_t	cp_ccbid;		/* ID of software CCB */
	u_int32_t	cp_dataaddr;		/* Addr of data/SG list */
	u_int32_t	cp_stataddr;		/* Addr of status packet */
	u_int32_t	cp_senseaddr;		/* Addr of req. sense */
} __packed;

struct eata_ucp {
	u_int8_t	ucp_cp[sizeof(struct eata_cp) - 5*4];	/* XXX */
	u_long		ucp_datalen;
	u_long		ucp_ccbid;
	void *		ucp_dataaddr;
	void *		ucp_stataddr;
	void *		ucp_senseaddr;
	u_long		ucp_timeout;
	u_int8_t	ucp_hstatus;
	u_int8_t	ucp_tstatus;
	u_int8_t	ucp_retries;
	u_int8_t	ucp_padding;
} __packed;

#define CP_C0_SCSI_RESET	0x01	/* Cause a bus reset */
#define CP_C0_HBA_INIT		0x02	/* Reinitialize HBA */
#define CP_C0_AUTO_SENSE	0x04	/* Auto request sense on error */
#define CP_C0_SCATTER		0x08	/* Do scatter/gather I/O */
#define CP_C0_QUICK		0x10	/* Return no status packet */
#define CP_C0_INTERPRET		0x20	/* HBA interprets SCSI CDB */
#define CP_C0_DATA_OUT		0x40	/* Data out phase */
#define CP_C0_DATA_IN		0x80	/* Data in phase */

#define CP_C1_TO_PHYS		0x01	/* Send to RAID component */
#define CP_C1_RESERVED		0xfe

#define CP_C2_PHYS_UNIT		0x01	/* Physical unit on mirrored pair */
#define CP_C2_NO_AT		0x02	/* No address translation */
#define CP_C2_NO_CACHE		0x04	/* No HBA caching */
#define CP_C2_RESERVED		0xf8

#define CP_C3_ID_MASK		0x1f	/* Target ID */
#define CP_C3_ID_SHIFT		0
#define CP_C3_CHANNEL_MASK	0xe0	/* Target channel */
#define CP_C3_CHANNEL_SHIFT	5

#define CP_C4_LUN_MASK		0x07	/* Target LUN */
#define CP_C4_LUN_SHIFT		0
#define CP_C4_RESERVED		0x18
#define CP_C4_LUN_TAR		0x20	/* CP is for target ROUTINE */
#define CP_C4_DIS_PRI		0x40	/* Give disconnect privilege */
#define CP_C4_IDENTIFY		0x80	/* Always true */

/*
 * EATA status packet as returned by controller upon command completion.  It
 * contains status, message info and a handle on the initiating CCB.
 */
struct eata_sp {
	u_int8_t	sp_hba_status;		/* Host adapter status */
	u_int8_t	sp_scsi_status;		/* SCSI bus status */
	u_int8_t	sp_reserved[2];		/* Reserved */
	u_int32_t	sp_inv_residue;		/* Bytes not transferred */
	u_int32_t	sp_ccbid;		/* ID of software CCB */
	u_int8_t	sp_id_message;
	u_int8_t	sp_que_message;
	u_int8_t	sp_tag_message;
	u_int8_t	sp_messages[9];
} __packed;

/*
 * HBA status as returned by status packet.  Bit 7 signals end of command.
 */
#define SP_HBA_NO_ERROR		0x00    /* No error on command */
#define SP_HBA_ERROR_SEL_TO	0x01    /* Device selection timeout */
#define SP_HBA_ERROR_CMD_TO	0x02    /* Device command timeout */
#define SP_HBA_ERROR_RESET	0x03    /* SCSI bus was reset */
#define SP_HBA_INIT_POWERUP	0x04    /* Initial controller power up */
#define SP_HBA_UNX_BUSPHASE	0x05    /* Unexpected bus phase */
#define SP_HBA_UNX_BUS_FREE	0x06    /* Unexpected bus free */
#define SP_HBA_BUS_PARITY	0x07    /* SCSI bus parity error */
#define SP_HBA_SCSI_HUNG	0x08    /* SCSI bus hung */
#define SP_HBA_UNX_MSGRJCT	0x09    /* Unexpected message reject */
#define SP_HBA_RESET_STUCK	0x0a    /* SCSI bus reset stuck */
#define SP_HBA_RSENSE_FAIL	0x0b    /* Auto-request sense failed */
#define SP_HBA_PARITY		0x0c    /* HBA memory parity error */
#define SP_HBA_ABORT_NA		0x0d    /* CP aborted - not on bus */
#define SP_HBA_ABORTED		0x0e    /* CP aborted - was on bus */
#define SP_HBA_RESET_NA		0x0f    /* CP reset - not on bus */
#define SP_HBA_RESET		0x10    /* CP reset - was on bus */
#define SP_HBA_ECC		0x11    /* HBA memory ECC error */
#define SP_HBA_PCI_PARITY	0x12    /* PCI parity error */
#define SP_HBA_PCI_MASTER	0x13    /* PCI master abort */
#define SP_HBA_PCI_TARGET	0x14    /* PCI target abort */
#define SP_HBA_PCI_SIG_TARGET	0x15    /* PCI signalled target abort */
#define SP_HBA_ABORT		0x20    /* Software abort (too many retries) */

/*
 * Scatter-gather list element.
 */
struct eata_sg {
	u_int32_t	sg_addr;
	u_int32_t	sg_len;
} __packed;

/*
 * EATA configuration data as returned by HBA.  XXX This is bogus - it
 * doesn't sync up with the structure FreeBSD uses. [ad]
 */
struct eata_cfg {
	u_int8_t	ec_devtype;
	u_int8_t	ec_pagecode;
	u_int8_t	ec_reserved0;
	u_int8_t	ec_cfglen;		/* Length in bytes past here */
	u_int8_t	ec_eatasig[4];		/* EATA signature */
	u_int8_t	ec_eataversion;		/* EATA version number */
	u_int8_t	ec_feat0;		/* First feature byte */
	u_int8_t	ec_padlength[2];	/* Pad bytes for PIO cmds */
	u_int8_t	ec_hba[4];		/* Host adapter SCSI IDs */
	u_int8_t	ec_cplen[4];		/* Command packet length */
	u_int8_t	ec_splen[4];		/* Status packet length */
	u_int8_t	ec_queuedepth[2];	/* Controller queue depth */
	u_int8_t	ec_reserved1[2];
	u_int8_t	ec_sglen[2];		/* Maximum s/g list size */
	u_int8_t	ec_feat1;		/* 2nd feature byte */
	u_int8_t	ec_irq;			/* IRQ address */
	u_int8_t	ec_feat2;		/* 3rd feature byte */
	u_int8_t	ec_feat3;		/* 4th feature byte */
	u_int8_t	ec_maxlun;		/* Maximum LUN supported */
	u_int8_t	ec_feat4;		/* 5th feature byte */
	u_int8_t	ec_raidnum;		/* RAID host adapter humber */
} __packed;

#define EC_F0_OVERLAP_CMDS	0x01	/* Overlapped cmds supported */
#define EC_F0_TARGET_MODE	0x02	/* Target mode supported */
#define EC_F0_TRUNC_NOT_REC	0x04	/* Truncate cmd not supported */
#define EC_F0_MORE_SUPPORTED	0x08	/* More cmd supported */
#define EC_F0_DMA_SUPPORTED	0x10	/* DMA mode supported */
#define EC_F0_DMA_NUM_VALID	0x20	/* DMA channel field is valid */
#define EC_F0_ATA_DEV		0x40	/* This is an ATA device */
#define EC_F0_HBA_VALID		0x80	/* HBA field is valid */

#define EC_F1_IRQ_NUM_MASK	0x0f	/* IRQ number mask */
#define EC_F1_IRQ_NUM_SHIFT	0
#define EC_F1_IRQ_TRIGGER	0x10	/* IRQ trigger: 0 = edge, 1 = level */
#define EC_F1_SECONDARY		0x20	/* Controller not at address 0x170 */
#define EC_F1_DMA_NUM_MASK	0xc0 	/* DMA channel *index* for ISA */
#define EC_F1_DMA_NUM_SHIFT	6

#define EC_F2_ISA_IO_DISABLE	0x01	/* ISA I/O address disabled */
#define EC_F2_FORCE_ADDR	0x02	/* HBA forced to EISA/ISA address */
#define EC_F2_SG_64K		0x04	/* 64kB of scatter/gather space */
#define EC_F2_SG_UNALIGNED	0x08	/* Can do unaligned scatter/gather */
#define EC_F2_RESERVED0		0x10	/* Reserved */
#define EC_F2_RESERVED1		0x20	/* Reserved */
#define EC_F2_RESERVED2		0x40	/* Reserved */
#define EC_F2_RESERVED3		0x40	/* Reserved */

#define EC_F3_MAX_TARGET_MASK	0x1f	/* Maximum target ID supported */
#define EC_F3_MAX_TARGET_SHIFT	0
#define EC_F3_MAX_CHANNEL_MASK	0xe0	/* Maximum channel ID supported */
#define EC_F3_MAX_CHANNEL_SHIFT	5

#define EC_F4_RESERVED0		0x01	/* Reserved */
#define EC_F4_RESERVED1		0x02	/* Reserved */
#define EC_F4_RESERVED2		0x04	/* Reserved */
#define EC_F4_AUTO_TERM		0x08	/* Supports auto termination */
#define EC_F4_PCIM1		0x10	/* PCI M1 chipset */
#define EC_F4_BOGUS_RAID_ID	0x20	/* RAID ID may be questionable  */
#define EC_F4_HBA_PCI		0x40	/* PCI adapter */
#define EC_F4_HBA_EISA		0x80	/* EISA adapter */

/*
 * How SCSI inquiry data breaks down for EATA boards.
 */
struct eata_inquiry_data {
	u_int8_t	ei_device;
	u_int8_t	ei_dev_qual2;
	u_int8_t	ei_version;
	u_int8_t 	ei_response_format;
	u_int8_t 	ei_additional_length;
	u_int8_t 	ei_unused[2];
	u_int8_t	ei_flags;
	char		ei_vendor[8];		/* Vendor, e.g: DPT, NEC */
	char		ei_model[7];		/* Model number */
	char		ei_suffix[9];		/* Model number suffix */
	char		ei_fw[3];		/* Firmware */
	char		ei_fwrev[1];		/* Firmware revision */
	u_int8_t	ei_extra[8];
} __packed;

#endif	/* !defined _IC_DPTREG_H_ */
