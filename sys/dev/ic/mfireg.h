/* $NetBSD: mfireg.h,v 1.8 2012/09/19 21:24:29 bouyer Exp $ */
/* $OpenBSD: mfireg.h,v 1.24 2006/06/19 19:05:45 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Copyright (c) 2007 LSI Corp.
 * Copyright (c) 2007 Rajesh Prabhakaran.
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
 */

#ifndef _DEV_IC_MFIREG_H_
#define _DEV_IC_MFIREG_H_

/* management interface constants */
#define MFI_MGMT_VD			0x01
#define MFI_MGMT_SD			0x02

/* generic constants */
#define MFI_FRAME_SIZE			64
#define MFI_SENSE_SIZE			128
#define MFI_OSTS_INTR_VALID		0x00000002 /* valid interrupt */
#define MFI_OSTS_PPC_INTR_VALID		0x80000000
#define MFI_OSTS_GEN2_INTR_VALID	(0x00000001 | 0x00000004)
#define MFI_INVALID_CTX			0xffffffff
#define MFI_ENABLE_INTR			0x01
#define MFI_MAXFER			MAXPHYS	/* XXX bogus */
#define MFI_SECTOR_LEN			512

/* register offsets */
#define MFI_IMSG0			0x10 /* inbound msg 0 */
#define MFI_IMSG1			0x14 /* inbound msg 1 */
#define MFI_OMSG0			0x18 /* outbound msg 0 */
#define MFI_OMSG1			0x1c /* outbound msg 1 */
#define MFI_IDB				0x20 /* inbound doorbell */
#define MFI_ISTS			0x24 /* inbound intr stat */
#define MFI_IMSK			0x28 /* inbound intr mask */
#define MFI_ODB				0x2c /* outbound doorbell */
#define MFI_OSTS			0x30 /* outbound intr stat */
#define MFI_OMSK			0x34 /* outbound inter mask */
#define MFI_IQP				0x40 /* inbound queue port */
#define MFI_OQP				0x44 /* outbound queue port */
#define MFI_ODC				0xa0 /* outbound doorbell clr */
#define MFI_OSP 			0xb0 /* outbound scratch pad */

/* ThunderBolt specific Register */
#define MFI_RPI				0x6c /* reply_post_host_index */
#define MFI_ILQP			0xc0 /* inbound_low_queue_port */
#define MFI_IHQP			0xc4 /* inbound_high_queue_port */

/* OCR registers */
#define MFI_WSR				0x004 /* write sequence register */
#define MFI_HDR				0x008 /* host diagnostic register */
#define MFI_RSR				0x3c3 /* Reset Status Register */

/* OCR specific flags */
#define MFI_FIRMWARE_STATE_CHANGE	0x00000002
#define MFI_STATE_CHANGE_INTERRUPT	0x00000004

/*
 * skinny specific changes
*/
#define MFI_SKINNY_IDB			0x00 /* Inbound doorbell is at 0x00 for skinny */
#define MFI_IQPL			0x000000c0
#define MFI_IQPH			0x000000c4
#define MFI_OSTS_SKINNY_INTR_VALID	0x00000001

/* * firmware states */
#define MFI_STATE_MASK			0xf0000000
#define MFI_STATE_UNDEFINED		0x00000000
#define MFI_STATE_BB_INIT		0x10000000
#define MFI_STATE_FW_INIT		0x40000000
#define MFI_STATE_WAIT_HANDSHAKE	0x60000000
#define MFI_STATE_FW_INIT_2		0x70000000
#define MFI_STATE_DEVICE_SCAN		0x80000000
#define MFI_STATE_FLUSH_CACHE		0xa0000000
#define MFI_STATE_READY			0xb0000000
#define MFI_STATE_OPERATIONAL		0xc0000000
#define MFI_STATE_FAULT			0xf0000000
#define MFI_STATE_MAXSGL_MASK		0x00ff0000
#define MFI_STATE_MAXCMD_MASK		0x0000ffff
#define MFI_STATE_HOSTMEMREQD_MASK	0x08000000
#define MFI_STATE_BOOT_MESSAGE_PENDING	0x90000000
#define MFI_RESET_REQUIRED		0x00000001

/* ThunderBolt Support */
#define MFI_STATE_TB_MASK		0xf0000000
#define MFI_STATE_TB_RESET		0x00000000
#define MFI_STATE_TB_READY		0x10000000
#define MFI_STATE_TB_OPERATIONAL	0x20000000
#define MFI_STATE_TB_FAULT		0x40000000

/* command reset register */
#define MFI_INIT_ABORT			0x00000000
#define MFI_INIT_READY			0x00000002
#define MFI_INIT_MFIMODE		0x00000004
#define MFI_INIT_CLEAR_HANDSHAKE	0x00000008
#define MFI_RESET_FLAGS			MFI_INIT_READY|MFI_INIT_MFIMODE
#define MFI_INIT_HOTPLUG		0x00000010

/* ADP reset flags */
#define MFI_STOP_ADP			0x00000020
#define MFI_ADP_RESET			0x00000040
#define DIAG_WRITE_ENABLE		0x00000080
#define DIAG_RESET_ADAPTER		0x00000004

/* mfi Frame flags */
#define MFI_FRAME_POST_IN_REPLY_QUEUE		0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE	0x0001
#define MFI_FRAME_SGL32				0x0000
#define MFI_FRAME_SGL64				0x0002
#define MFI_FRAME_SENSE32			0x0000
#define MFI_FRAME_SENSE64			0x0004
#define MFI_FRAME_DIR_NONE			0x0000
#define MFI_FRAME_DIR_WRITE			0x0008
#define MFI_FRAME_DIR_READ			0x0010
#define MFI_FRAME_DIR_BOTH			0x0018
#define MFI_FRAME_IEEE_SGL			0x0020

/* ThunderBolt Specific */

/*
 * Pre-TB command size and TB command size.
 * We will be checking it at the load time for the time being
 */
#define MR_COMMAND_SIZE (MFI_FRAME_SIZE*20) /* 1280 bytes */

#define MEGASAS_THUNDERBOLT_MSG_ALLIGNMENT	256
/*
 * We are defining only 128 byte message to reduce memory move over head
 * and also it will reduce the SRB extension size by 128byte compared with
 * 256 message size
 */
#define MEGASAS_THUNDERBOLT_NEW_MSG_SIZE	256
#define MEGASAS_THUNDERBOLT_MAX_COMMANDS	1024
#define MEGASAS_THUNDERBOLT_MAX_REPLY_COUNT	1024
#define MEGASAS_THUNDERBOLT_REPLY_SIZE		8
#define MEGASAS_THUNDERBOLT_MAX_CHAIN_COUNT	1
#define MEGASAS_MAX_SZ_CHAIN_FRAME		1024

/*
 * Calculating how many SGEs allowed in a allocated main message
 * (size of the Message - Raid SCSI IO message size(except SGE))
 * / size of SGE
 * (0x100 - (0x90 - 0x10)) / 0x10 = 8
 */
#define MEGASAS_THUNDERBOLT_MAX_SGE_IN_MAINMSG \
 ((MEGASAS_THUNDERBOLT_NEW_MSG_SIZE - \
   (sizeof(struct mfi_mpi2_request_raid_scsi_io) - sizeof(mpi2_sge_io_union))\
  ) / sizeof(mpi2_sge_io_union))
 
/* 
 * (Command frame size allocaed in SRB ext - Raid SCSI IO message size)
 * / size of SGL ; 
 * (1280 - 256) / 16 = 64
 */   
#define MEGASAS_THUNDERBOLT_MAX_SGE_IN_CHAINMSG \
  ((MR_COMMAND_SIZE - MEGASAS_THUNDERBOLT_NEW_MSG_SIZE) / \
   sizeof(mpi2_sge_io_union))

/*
 * This is the offset in number of 4 * 32bit words to the next chain
 * (0x100 - 0x10)/0x10 = 0xF(15)
 */
#define MEGASAS_THUNDERBOLT_CHAIN_OFF_MAINMSG \
  ((MEGASAS_THUNDERBOLT_NEW_MSG_SIZE - sizeof(mpi2_sge_io_union)) / 16)

#define MEGASAS_THUNDERBOLT_CHAIN_OFF_MPT_PTMSG \
  (offsetof(struct mfi_mpi2_request_raid_scsi_io, SGL) / 16)

#define MPI2_FUNCTION_PASSTHRU_IO_REQUEST	0xF0
#define MPI2_FUNCTION_LD_IO_REQUEST		0xF1

#define MR_INTERNAL_MFI_FRAMES_SMID		1
#define MR_CTRL_EVENT_WAIT_SMID			2
#define MR_INTERNAL_DRIVER_RESET_SMID		3

/* mfi command opcodes */
#define MFI_CMD_INIT			0x00
#define MFI_CMD_LD_READ			0x01
#define MFI_CMD_LD_WRITE		0x02
#define MFI_CMD_LD_SCSI_IO		0x03
#define MFI_CMD_PD_SCSI_IO		0x04
#define MFI_CMD_DCMD			0x05
#define MFI_CMD_ABORT			0x06
#define MFI_CMD_SMP			0x07
#define MFI_CMD_STP			0x08

/* direct commands */
#define MR_DCMD_CTRL_GET_INFO		0x01010000
#define MR_DCMD_CTRL_CACHE_FLUSH	0x01101000
#define   MR_FLUSH_CTRL_CACHE		0x01
#define   MR_FLUSH_DISK_CACHE		0x02
#define MR_DCMD_CTRL_HOST_MEM_ALLOC	0x0100e100
#define MR_DCMD_CTRL_SHUTDOWN		0x01050000
#define   MR_ENABLE_DRIVE_SPINDOWN	0x01
#define MR_DCMD_CTRL_EVENT_GET_INFO	0x01040100
#define MR_DCMD_CTRL_EVENT_GET		0x01040300
#define MR_DCMD_CTRL_EVENT_WAIT		0x01040500
#define MR_DCMD_PD_GET_LIST		0x02010000
#define MR_DCMD_PD_LIST_QUERY		0x02010100
#define MR_DCMD_PD_GET_INFO		0x02020000
#define MD_DCMD_PD_SET_STATE		0x02030100
#define MD_DCMD_PD_REBUILD		0x02040100
#define MR_DCMD_PD_BLINK		0x02070100
#define MR_DCMD_PD_UNBLINK		0x02070200
#define MR_DCMD_LD_MAP_GET_INFO		0x0300e101
#define MR_DCMD_LD_SYNC			0x0300e102
#define MR_DCMD_LD_GET_LIST		0x03010000
#define MR_DCMD_LD_GET_INFO		0x03020000
#define MR_DCMD_LD_GET_PROPERTIES	0x03030000
#define MD_DCMD_CONF_GET		0x04010000
#define MR_DCMD_BBU_GET_STATUS		0x05010000
#define MR_DCMD_BBU_GET_CAPACITY_INFO	0x05020000
#define MR_DCMD_BBU_GET_DESIGN_INFO	0x05030000
#define MR_DCMD_CLUSTER			0x08000000
#define MR_DCMD_CLUSTER_RESET_ALL	0x08010100
#define MR_DCMD_CLUSTER_RESET_LD	0x08010200

#define MR_DCMD_SPEAKER_GET		0x01030100
#define MR_DCMD_SPEAKER_ENABLE		0x01030200
#define MR_DCMD_SPEAKER_DISABLE		0x01030300
#define MR_DCMD_SPEAKER_SILENCE		0x01030400
#define MR_DCMD_SPEAKER_TEST		0x01030500

/* mailbox bytes in direct command */
#define MFI_MBOX_SIZE			12

/* mfi completion codes */
typedef enum {
	MFI_STAT_OK =				0x00,
	MFI_STAT_INVALID_CMD =			0x01,
	MFI_STAT_INVALID_DCMD =			0x02,
	MFI_STAT_INVALID_PARAMETER =		0x03,
	MFI_STAT_INVALID_SEQUENCE_NUMBER =	0x04,
	MFI_STAT_ABORT_NOT_POSSIBLE =		0x05,
	MFI_STAT_APP_HOST_CODE_NOT_FOUND =	0x06,
	MFI_STAT_APP_IN_USE =			0x07,
	MFI_STAT_APP_NOT_INITIALIZED =		0x08,
	MFI_STAT_ARRAY_INDEX_INVALID =		0x09,
	MFI_STAT_ARRAY_ROW_NOT_EMPTY =		0x0a,
	MFI_STAT_CONFIG_RESOURCE_CONFLICT =	0x0b,
	MFI_STAT_DEVICE_NOT_FOUND =		0x0c,
	MFI_STAT_DRIVE_TOO_SMALL =		0x0d,
	MFI_STAT_FLASH_ALLOC_FAIL =		0x0e,
	MFI_STAT_FLASH_BUSY =			0x0f,
	MFI_STAT_FLASH_ERROR =			0x10,
	MFI_STAT_FLASH_IMAGE_BAD =		0x11,
	MFI_STAT_FLASH_IMAGE_INCOMPLETE =	0x12,
	MFI_STAT_FLASH_NOT_OPEN =		0x13,
	MFI_STAT_FLASH_NOT_STARTED =		0x14,
	MFI_STAT_FLUSH_FAILED =			0x15,
	MFI_STAT_HOST_CODE_NOT_FOUNT =		0x16,
	MFI_STAT_LD_CC_IN_PROGRESS =		0x17,
	MFI_STAT_LD_INIT_IN_PROGRESS =		0x18,
	MFI_STAT_LD_LBA_OUT_OF_RANGE =		0x19,
	MFI_STAT_LD_MAX_CONFIGURED =		0x1a,
	MFI_STAT_LD_NOT_OPTIMAL =		0x1b,
	MFI_STAT_LD_RBLD_IN_PROGRESS =		0x1c,
	MFI_STAT_LD_RECON_IN_PROGRESS =		0x1d,
	MFI_STAT_LD_WRONG_RAID_LEVEL =		0x1e,
	MFI_STAT_MAX_SPARES_EXCEEDED =		0x1f,
	MFI_STAT_MEMORY_NOT_AVAILABLE =		0x20,
	MFI_STAT_MFC_HW_ERROR =			0x21,
	MFI_STAT_NO_HW_PRESENT =		0x22,
	MFI_STAT_NOT_FOUND =			0x23,
	MFI_STAT_NOT_IN_ENCL =			0x24,
	MFI_STAT_PD_CLEAR_IN_PROGRESS =		0x25,
	MFI_STAT_PD_TYPE_WRONG =		0x26,
	MFI_STAT_PR_DISABLED =			0x27,
	MFI_STAT_ROW_INDEX_INVALID =		0x28,
	MFI_STAT_SAS_CONFIG_INVALID_ACTION =	0x29,
	MFI_STAT_SAS_CONFIG_INVALID_DATA =	0x2a,
	MFI_STAT_SAS_CONFIG_INVALID_PAGE =	0x2b,
	MFI_STAT_SAS_CONFIG_INVALID_TYPE =	0x2c,
	MFI_STAT_SCSI_DONE_WITH_ERROR =		0x2d,
	MFI_STAT_SCSI_IO_FAILED =		0x2e,
	MFI_STAT_SCSI_RESERVATION_CONFLICT =	0x2f,
	MFI_STAT_SHUTDOWN_FAILED =		0x30,
	MFI_STAT_TIME_NOT_SET =			0x31,
	MFI_STAT_WRONG_STATE =			0x32,
	MFI_STAT_LD_OFFLINE =			0x33,
	MFI_STAT_PEER_NOTIFICATION_REJECTED =	0x34,
	MFI_STAT_PEER_NOTIFICATION_FAILED =	0x35,
	MFI_STAT_RESERVATION_IN_PROGRESS =	0x36,
	MFI_STAT_I2C_ERRORS_DETECTED =		0x37,
	MFI_STAT_PCI_ERRORS_DETECTED =		0x38,
	MFI_STAT_INVALID_STATUS =		0xff
} mfi_status_t;

typedef enum {
	MFI_EVT_CLASS_DEBUG =			-2,
	MFI_EVT_CLASS_PROGRESS =		-1,
	MFI_EVT_CLASS_INFO =			0,
	MFI_EVT_CLASS_WARNING =			1,
	MFI_EVT_CLASS_CRITICAL =		2,
	MFI_EVT_CLASS_FATAL =			3,
	MFI_EVT_CLASS_DEAD =			4
} mfi_evt_class_t;

typedef enum {
	MFI_EVT_LOCALE_LD =			0x0001,
	MFI_EVT_LOCALE_PD =			0x0002,
	MFI_EVT_LOCALE_ENCL =			0x0004,
	MFI_EVT_LOCALE_BBU =			0x0008,
	MFI_EVT_LOCALE_SAS =			0x0010,
	MFI_EVT_LOCALE_CTRL =			0x0020,
	MFI_EVT_LOCALE_CONFIG =			0x0040,
	MFI_EVT_LOCALE_CLUSTER =		0x0080,
	MFI_EVT_LOCALE_ALL =			0xffff
} mfi_evt_locale_t;

typedef enum {
        MR_EVT_ARGS_NONE =			0x00,
        MR_EVT_ARGS_CDB_SENSE,
        MR_EVT_ARGS_LD,
        MR_EVT_ARGS_LD_COUNT,
        MR_EVT_ARGS_LD_LBA,
        MR_EVT_ARGS_LD_OWNER,
        MR_EVT_ARGS_LD_LBA_PD_LBA,
        MR_EVT_ARGS_LD_PROG,
        MR_EVT_ARGS_LD_STATE,
        MR_EVT_ARGS_LD_STRIP,
        MR_EVT_ARGS_PD,
        MR_EVT_ARGS_PD_ERR,
        MR_EVT_ARGS_PD_LBA,
        MR_EVT_ARGS_PD_LBA_LD,
        MR_EVT_ARGS_PD_PROG,
        MR_EVT_ARGS_PD_STATE,
        MR_EVT_ARGS_PCI,
        MR_EVT_ARGS_RATE,
        MR_EVT_ARGS_STR,
        MR_EVT_ARGS_TIME,
        MR_EVT_ARGS_ECC
} mfi_evt_args;

/* XXX should be in mfi_evt_args ? */
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED	0x0152
#define MR_EVT_PD_REMOVED			0x0070
#define MR_EVT_PD_INSERTED			0x005b
#define MR_EVT_LD_CHANGE			0x0051

typedef enum {
	MR_PD_QUERY_TYPE_ALL =			0,
	MR_PD_QUERY_TYPE_STATE =		1,
	MR_PD_QUERY_TYPE_POWER_STATE =		2,
	MR_PD_QUERY_TYPE_MEDIA_TYPE =		3,
	MR_PD_QUERY_TYPE_SPEED =		4,
	MR_PD_QUERY_TYPE_EXPOSED_TO_HOST =	5 /*query for system drives */
} mfi_pd_query_type;

/* driver definitions */
#define MFI_MAX_PD_CHANNELS			2
#define MFI_MAX_PD_ARRAY			32
#define MFI_MAX_LD_CHANNELS			2
#define MFI_MAX_CHANNELS	(MFI_MAX_PD_CHANNELS + MFI_MAX_LD_CHANNELS)
#define MFI_MAX_CHANNEL_DEVS			128
#define MFI_DEFAULT_ID				-1
#define MFI_MAX_LUN				8
#define MFI_MAX_LD				64
#define MFI_MAX_SPAN				8
#define MFI_MAX_ARRAY_DEDICATED			16

/* sense buffer */
struct mfi_sense {
	uint8_t			mse_data[MFI_SENSE_SIZE];
} __packed;

/* scatter gather elements */
struct mfi_sg32 {
	uint32_t		addr;
	uint32_t		len;
} __packed;

struct mfi_sg64 {
	uint64_t		addr;
	uint32_t		len;
} __packed;

struct mfi_sg_ieee {
	uint64_t		addr;
	uint32_t		len;
	uint32_t		flags;
} __packed;


union mfi_sgl {
	struct mfi_sg32		sg32[1];
	struct mfi_sg64		sg64[1];
	struct mfi_sg_ieee	sg_ieee[1];
} __packed;

/* message frame */
struct mfi_frame_header {
	uint8_t			mfh_cmd;
	uint8_t			mfh_sense_len;
	uint8_t			mfh_cmd_status;
	uint8_t			mfh_scsi_status;
	uint8_t			mfh_target_id;
	uint8_t			mfh_lun_id;
	uint8_t			mfh_cdb_len;
	uint8_t			mfh_sg_count;
	uint32_t		mfh_context;
	uint32_t		mfh_pad0;
	uint16_t		mfh_flags;
	uint16_t		mfh_timeout;
	uint32_t		mfh_data_len;
} __packed;

union mfi_sgl_frame {
	struct mfi_sg32		sge32[8];
	struct mfi_sg64		sge64[5];

} __packed;

struct mfi_init_frame {
	struct mfi_frame_header	mif_header;
	uint32_t		mif_qinfo_new_addr_lo;
	uint32_t		mif_qinfo_new_addr_hi;
	uint32_t		mif_qinfo_old_addr_lo;
	uint32_t		mif_qinfo_old_addr_hi;
	uint32_t		driver_ver_lo;	/* 0x28 */
	uint32_t		driver_ver_hi;	/* 0x2c */
	uint32_t		reserved[4];
} __packed;

/* queue init structure */
struct mfi_init_qinfo {
	uint32_t		miq_flags;
	uint32_t		miq_rq_entries;
	uint32_t		miq_rq_addr_lo;
	uint32_t		miq_rq_addr_hi;
	uint32_t		miq_pi_addr_lo;
	uint32_t		miq_pi_addr_hi;
	uint32_t		miq_ci_addr_lo;
	uint32_t		miq_ci_addr_hi;
} __packed;

#define MFI_IO_FRAME_SIZE	40
struct mfi_io_frame {
	struct mfi_frame_header	mif_header;
	uint32_t		mif_sense_addr_lo;
	uint32_t		mif_sense_addr_hi;
	uint32_t		mif_lba_lo;
	uint32_t		mif_lba_hi;
	union mfi_sgl		mif_sgl;
} __packed;

#define MFI_PASS_FRAME_SIZE	48
struct mfi_pass_frame {
	struct mfi_frame_header mpf_header;
	uint32_t		mpf_sense_addr_lo;
	uint32_t		mpf_sense_addr_hi;
	uint8_t			mpf_cdb[16];
	union mfi_sgl		mpf_sgl;
} __packed;

#define MFI_DCMD_FRAME_SIZE	40
struct mfi_dcmd_frame {
	struct mfi_frame_header mdf_header;
	uint32_t		mdf_opcode;
	uint8_t			mdf_mbox[MFI_MBOX_SIZE];
	union mfi_sgl		mdf_sgl;
} __packed;
#define MFI_DCMD_MBOX_PEND_FLAG	0x1

struct mfi_abort_frame {
	struct mfi_frame_header maf_header;
	uint32_t		maf_abort_context;
	uint32_t		maf_pad;
	uint32_t		maf_abort_mfi_addr_lo;
	uint32_t		maf_abort_mfi_addr_hi;
	uint32_t		maf_reserved[6];
} __packed;

struct mfi_smp_frame {
	struct mfi_frame_header msf_header;
	uint64_t		msf_sas_addr;
	union {
		struct mfi_sg32 sg32[2];
		struct mfi_sg64 sg64[2];
	}			msf_sgl;
} __packed;

struct mfi_stp_frame {
	struct mfi_frame_header msf_header;
	uint16_t		msf_fis[10];
	uint32_t		msf_stp_flags;
	union {
		struct mfi_sg32 sg32[2];
		struct mfi_sg64 sg64[2];
	} 			msf_sgl;
} __packed;

union mfi_frame {
	struct mfi_frame_header mfr_header;
	struct mfi_init_frame	mfr_init;
	struct mfi_io_frame	mfr_io;
	struct mfi_pass_frame	mfr_pass;
	struct mfi_dcmd_frame	mfr_dcmd;
	struct mfi_abort_frame	mfr_abort;
	struct mfi_smp_frame	mfr_smp;
	struct mfi_stp_frame	mfr_stp;
	uint8_t			mfr_bytes[MFI_FRAME_SIZE];
};

union mfi_evt_class_locale {
	struct {
		uint16_t	locale;
		uint8_t 	reserved;
		int8_t		class;
	} __packed		mec_members;

	uint32_t		mec_word;
} __packed;

struct mfi_evt_log_info {
	uint32_t		mel_newest_seq_num;
	uint32_t		mel_oldest_seq_num;
	uint32_t		mel_clear_seq_num;
	uint32_t		mel_shutdown_seq_num;
	uint32_t		mel_boot_seq_num;
} __packed;

struct mfi_progress {
	uint16_t		mp_progress;
	uint16_t		mp_elapsed_seconds;
} __packed;

struct mfi_evtarg_ld {
	uint16_t		mel_target_id;
	uint8_t			mel_ld_index;
	uint8_t			mel_reserved;
} __packed;

struct mfi_evtarg_pd {
	uint16_t		mep_device_id;
	uint8_t			mep_encl_index;
	uint8_t			mep_slot_number;
} __packed;

struct mfi_evt_detail {
	uint32_t				med_seq_num;
	uint32_t				med_time_stamp;
	uint32_t				med_code;
	union mfi_evt_class_locale		med_cl;
	uint8_t					med_arg_type;
	uint8_t					med_reserved1[15];

	union {
		struct {
			struct mfi_evtarg_pd	pd;
			uint8_t			cdb_length;
			uint8_t			sense_length;
			uint8_t			reserved[2];
			uint8_t			cdb[16];
			uint8_t			sense[64];
		} __packed			cdb_sense;

		struct mfi_evtarg_ld 		ld;

		struct {
			struct mfi_evtarg_ld	ld;
			uint64_t		count;
		} __packed			ld_count;

		struct {
			uint64_t		lba;
			struct mfi_evtarg_ld	ld;
		} __packed			ld_lba;

		struct {
			struct mfi_evtarg_ld	ld;
			uint32_t		prev_owner;
			uint32_t		new_owner;
		} __packed			ld_owner;

		struct {
			uint64_t		ld_lba;
			uint64_t		pd_lba;
			struct mfi_evtarg_ld	ld;
			struct mfi_evtarg_pd	pd;
		} __packed			ld_lba_pd_lba;

		struct {
			struct mfi_evtarg_ld	ld;
			struct mfi_progress	prog;
		} __packed			ld_prog;

		struct {
			struct mfi_evtarg_ld	ld;
			uint32_t		prev_state;
			uint32_t		new_state;
		} __packed			ld_state;

		struct {
			uint64_t		strip;
			struct mfi_evtarg_ld	ld;
		} __packed			ld_strip;

		struct mfi_evtarg_pd		pd;

		struct {
			struct mfi_evtarg_pd	pd;
			uint32_t		err;
		} __packed			pd_err;

		struct {
			uint64_t		lba;
			struct mfi_evtarg_pd	pd;
		} __packed			pd_lba;

		struct {
			uint64_t		lba;
			struct mfi_evtarg_pd	pd;
			struct mfi_evtarg_ld	ld;
		} __packed			pd_lba_ld;

		struct {
			struct mfi_evtarg_pd	pd;
			struct mfi_progress	prog;
		} __packed			pd_prog;

		struct {
			struct mfi_evtarg_pd	pd;
			uint32_t		prev_state;
			uint32_t		new_state;
		} __packed			pd_state;

		struct {
			uint16_t		vendor_id;
			uint16_t		device_id;
			uint16_t		subvendor_id;
			uint16_t		subdevice_id;
		} __packed			pci;

		uint32_t			rate;
		char				str[96];

		struct {
			uint32_t		rtc;
			uint32_t		elapsed_seconds;
		} __packed			time;

		struct {
			uint32_t		ecar;
			uint32_t		elog;
			char			str[64];
		} __packed			ecc;

		uint8_t				b[96];
		uint16_t			s[48];
		uint32_t			w[24];
		uint64_t			d[12];
	}					args;

	char					med_description[128];
} __packed;

/* controller properties from mfi_ctrl_info */
struct mfi_ctrl_props {
	uint16_t		mcp_seq_num;
	uint16_t		mcp_pred_fail_poll_interval;
	uint16_t		mcp_intr_throttle_cnt;
	uint16_t		mcp_intr_throttle_timeout;
	uint8_t			mcp_rebuild_rate;
	uint8_t			mcp_patrol_read_rate;
	uint8_t			mcp_bgi_rate;
	uint8_t			mcp_cc_rate;
	uint8_t			mcp_recon_rate;
	uint8_t			mcp_cache_flush_interval;
	uint8_t			mcp_spinup_drv_cnt;
	uint8_t			mcp_spinup_delay;
	uint8_t			mcp_cluster_enable;
	uint8_t			mcp_coercion_mode;
	uint8_t			mcp_alarm_enable;
	uint8_t			mcp_disable_auto_rebuild;
	uint8_t			mcp_disable_battery_warn;
	uint8_t			mcp_ecc_bucket_size;
	uint16_t		mcp_ecc_bucket_leak_rate;
	uint8_t			mcp_restore_hotspare_on_insertion;
	uint8_t			mcp_expose_encl_devices;
	uint8_t	 		maintainPdFailHistory;
	uint8_t	 		disallowHostRequestReordering;
	/* set TRUE to abort CC on detecting an inconsistency */
	uint8_t	 		abortCCOnError;
	/* load balance mode (MR_LOAD_BALANCE_MODE) */
	uint8_t	 		loadBalanceMode;
	/*
	 * 0 - use auto detect logic of backplanes like SGPIO, i2c SEP using
	 *     h/w mechansim like GPIO pins
	 * 1 - disable auto detect SGPIO,
	 * 2 - disable i2c SEP auto detect
	 * 3 - disable both auto detect
	 */
	uint8_t	 		disableAutoDetectBackplane;
	/*
	 * % of source LD to be reserved for a VDs snapshot in snapshot
	 * repository, for metadata and user data: 1=5%, 2=10%, 3=15% and so on
	 */
	uint8_t	 		snapVDSpace;

	/*
	 * Add properties that can be controlled by a bit in the following
	 * structure.
	 */
	struct {
		/* set TRUE to disable copyBack (0=copback enabled) */
		uint32_t	copyBackDisabled		:1;
		uint32_t	SMARTerEnabled		  	:1;
		uint32_t	prCorrectUnconfiguredAreas      :1;
		uint32_t	useFdeOnly		      	:1;
		uint32_t	disableNCQ		      	:1;
		uint32_t	SSDSMARTerEnabled	       	:1;
		uint32_t	SSDPatrolReadEnabled	    	:1;
		uint32_t	enableSpinDownUnconfigured      :1;
		uint32_t	autoEnhancedImport	      	:1;
		uint32_t	enableSecretKeyControl	  	:1;
		uint32_t	disableOnlineCtrlReset	  	:1;
		uint32_t	allowBootWithPinnedCache	:1;
		uint32_t	disableSpinDownHS	       	:1;
		uint32_t	enableJBOD		      	:1;
		uint32_t	reserved			:18;
	} OnOffProperties;
	/*
	 * % of source LD to be reserved for auto snapshot in snapshot
	 * repository, for metadata and user data: 1=5%, 2=10%, 3=15% and so on.
	 */
	uint8_t			autoSnapVDSpace;
	/*
	 * Snapshot writeable VIEWs capacity as a % of source LD capacity:
	 * 0=READ only, 1=5%, 2=10%, 3=15% and so on.
	 */
	uint8_t			viewSpace;
	/* # of idle minutes before device is spun down (0=use FW defaults) */
	uint16_t		spinDownTime;
	uint8_t			reserved[24];
} __packed;

/* pci info */
struct mfi_info_pci {
	uint16_t		mip_vendor;
	uint16_t		mip_device;
	uint16_t		mip_subvendor;
	uint16_t		mip_subdevice;
	uint8_t			mip_reserved[24];
} __packed;

/* host interface infor */
struct mfi_info_host {
	uint8_t			mih_type;
#define MFI_INFO_HOST_PCIX	0x01
#define MFI_INFO_HOST_PCIE	0x02
#define MFI_INFO_HOST_ISCSI	0x04
#define MFI_INFO_HOST_SAS3G	0x08
	uint8_t			mih_reserved[6];
	uint8_t			mih_port_count;
	uint64_t		mih_port_addr[8];
} __packed;

/* device  interface info */
struct mfi_info_device {
	uint8_t			mid_type;
#define MFI_INFO_DEV_SPI	0x01
#define MFI_INFO_DEV_SAS3G	0x02
#define MFI_INFO_DEV_SATA1	0x04
#define MFI_INFO_DEV_SATA3G	0x08
	uint8_t			mid_reserved[6];
	uint8_t			mid_port_count;
	uint64_t		mid_port_addr[8];
} __packed;

/* firmware component info */
struct mfi_info_component {
	char		 	mic_name[8];
	char		 	mic_version[32];
	char		 	mic_build_date[16];
	char		 	mic_build_time[16];
} __packed;

/* controller info from MFI_DCMD_CTRL_GETINFO. */
struct mfi_ctrl_info {
	struct mfi_info_pci	mci_pci;
	struct mfi_info_host	mci_host;
	struct mfi_info_device	mci_device;

	/* Firmware components that are present and active. */
	uint32_t		mci_image_check_word;
	uint32_t		mci_image_component_count;
	struct mfi_info_component mci_image_component[8];

	/* Firmware components that have been flashed but are inactive */
	uint32_t		mci_pending_image_component_count;
	struct mfi_info_component mci_pending_image_component[8];

	uint8_t			mci_max_arms;
	uint8_t			mci_max_spans;
	uint8_t			mci_max_arrays;
	uint8_t			mci_max_lds;
	char			mci_product_name[80];
	char			mci_serial_number[32];
	uint32_t		mci_hw_present;
#define MFI_INFO_HW_BBU		0x01
#define MFI_INFO_HW_ALARM	0x02
#define MFI_INFO_HW_NVRAM	0x04
#define MFI_INFO_HW_UART	0x08
	uint32_t		mci_current_fw_time;
	uint16_t		mci_max_cmds;
	uint16_t		mci_max_sg_elements;
	uint32_t		mci_max_request_size;
	uint16_t		mci_lds_present;
	uint16_t		mci_lds_degraded;
	uint16_t		mci_lds_offline;
	uint16_t		mci_pd_present;
	uint16_t		mci_pd_disks_present;
	uint16_t		mci_pd_disks_pred_failure;
	uint16_t		mci_pd_disks_failed;
	uint16_t		mci_nvram_size;
	uint16_t		mci_memory_size;
	uint16_t		mci_flash_size;
	uint16_t		mci_ram_correctable_errors;
	uint16_t		mci_ram_uncorrectable_errors;
	uint8_t			mci_cluster_allowed;
	uint8_t			mci_cluster_active;
	uint16_t		mci_max_strips_per_io;

	uint32_t		mci_raid_levels;
#define MFI_INFO_RAID_0		0x01
#define MFI_INFO_RAID_1		0x02
#define MFI_INFO_RAID_5		0x04
#define MFI_INFO_RAID_1E	0x08
#define MFI_INFO_RAID_6		0x10

	uint32_t		mci_adapter_ops;
#define MFI_INFO_AOPS_RBLD_RATE		0x0001		
#define MFI_INFO_AOPS_CC_RATE		0x0002
#define MFI_INFO_AOPS_BGI_RATE		0x0004
#define MFI_INFO_AOPS_RECON_RATE	0x0008
#define MFI_INFO_AOPS_PATROL_RATE	0x0010
#define MFI_INFO_AOPS_ALARM_CONTROL	0x0020
#define MFI_INFO_AOPS_CLUSTER_SUPPORTED	0x0040
#define MFI_INFO_AOPS_BBU		0x0080
#define MFI_INFO_AOPS_SPANNING_ALLOWED	0x0100
#define MFI_INFO_AOPS_DEDICATED_SPARES	0x0200
#define MFI_INFO_AOPS_REVERTIBLE_SPARES	0x0400
#define MFI_INFO_AOPS_FOREIGN_IMPORT	0x0800
#define MFI_INFO_AOPS_SELF_DIAGNOSTIC	0x1000
#define MFI_INFO_AOPS_MIXED_ARRAY	0x2000
#define MFI_INFO_AOPS_GLOBAL_SPARES	0x4000

	uint32_t		mci_ld_ops;
#define MFI_INFO_LDOPS_READ_POLICY	0x01
#define MFI_INFO_LDOPS_WRITE_POLICY	0x02
#define MFI_INFO_LDOPS_IO_POLICY	0x04
#define MFI_INFO_LDOPS_ACCESS_POLICY	0x08
#define MFI_INFO_LDOPS_DISK_CACHE_POLICY 0x10

	struct {
		uint8_t		min;
		uint8_t		max;
		uint8_t		reserved[2];
	} __packed		mci_stripe_sz_ops;

	uint32_t		mci_pd_ops;
#define MFI_INFO_PDOPS_FORCE_ONLINE	0x01
#define MFI_INFO_PDOPS_FORCE_OFFLINE	0x02
#define MFI_INFO_PDOPS_FORCE_REBUILD	0x04

	uint32_t		mci_pd_mix_support;
#define MFI_INFO_PDMIX_SAS		0x01
#define MFI_INFO_PDMIX_SATA		0x02
#define MFI_INFO_PDMIX_ENCL		0x04
#define MFI_INFO_PDMIX_LD		0x08
#define MFI_INFO_PDMIX_SATA_CLUSTER	0x10

	uint8_t			mci_ecc_bucket_count;
	uint8_t			mci_reserved2[11];
	struct mfi_ctrl_props	mci_properties;
	char			mci_package_version[0x60];
	uint8_t			mci_pad[0x800 - 0x6a0];
} __packed;

/* logical disk info from MR_DCMD_LD_GET_LIST */
struct mfi_ld {
	uint8_t			mld_target;
	uint8_t			mld_res;
	uint16_t		mld_seq;
} __packed;

struct mfi_ld_list {
	uint32_t		mll_no_ld;
	uint32_t		mll_res;
	struct {
		struct mfi_ld	mll_ld;
		uint8_t		mll_state;
#define MFI_LD_OFFLINE			0x00
#define MFI_LD_PART_DEGRADED		0x01
#define MFI_LD_DEGRADED			0x02
#define MFI_LD_ONLINE			0x03
		uint8_t		mll_res2;
		uint8_t		mll_res3;
		uint8_t		mll_res4;
		u_quad_t	mll_size;
	} mll_list[MFI_MAX_LD];
} __packed;

/* logicl disk details from MR_DCMD_LD_GET_INFO */
struct mfi_ld_prop {
	struct mfi_ld		mlp_ld;
	char			mlp_name[16];
	uint8_t			mlp_cache_policy;
	uint8_t			mlp_acces_policy;
	uint8_t			mlp_diskcache_policy;
	uint8_t			mlp_cur_cache_policy;
	uint8_t			mlp_disable_bgi;
	uint8_t			mlp_res[7];
} __packed;

struct mfi_ld_parm {
	uint8_t			mpa_pri_raid;	/* SNIA DDF PRL */
#define MFI_DDF_PRL_RAID0	0x00
#define MFI_DDF_PRL_RAID1	0x01
#define MFI_DDF_PRL_RAID3	0x03
#define MFI_DDF_PRL_RAID4	0x04
#define MFI_DDF_PRL_RAID5	0x05
#define MFI_DDF_PRL_RAID1E	0x11
#define MFI_DDF_PRL_JBOD	0x0f
#define MFI_DDF_PRL_CONCAT	0x1f
#define MFI_DDF_PRL_RAID5E	0x15
#define MFI_DDF_PRL_RAID5EE	0x25
#define MFI_DDF_PRL_RAID6	0x16
	uint8_t			mpa_raid_qual;	/* SNIA DDF RLQ */
	uint8_t			mpa_sec_raid;	/* SNIA DDF SRL */
#define MFI_DDF_SRL_STRIPED	0x00
#define MFI_DDF_SRL_MIRRORED	0x01
#define MFI_DDF_SRL_CONCAT	0x02
#define MFI_DDF_SRL_SPANNED	0x03
	uint8_t			mpa_stripe_size;
	uint8_t			mpa_no_drv_per_span;
	uint8_t			mpa_span_depth;
	uint8_t			mpa_state;
	uint8_t			mpa_init_state;
	uint8_t			mpa_is_consistent;
	uint8_t			mpa_res1[6];
	uint8_t			mpa_isSSCD;
	uint8_t			mpa_res[16];
} __packed;

struct mfi_ld_span {
	u_quad_t		mls_start_block;
	u_quad_t		mls_no_blocks;
	uint16_t		mls_index;
	uint8_t			mls_res[6];
} __packed;

struct mfi_ld_cfg {
	struct mfi_ld_prop	mlc_prop;
	struct mfi_ld_parm	mlc_parm;
	struct mfi_ld_span	mlc_span[MFI_MAX_SPAN];
} __packed;

struct mfi_ld_progress {
	uint32_t		mlp_in_prog;
#define MFI_LD_PROG_CC		0x01
#define MFI_LD_PROG_BGI		0x02
#define MFI_LD_PROG_FGI		0x04
#define MFI_LD_PROG_RECONSTRUCT	0x08
	struct mfi_progress	mlp_cc;
	struct mfi_progress	mlp_bgi;
	struct mfi_progress	mlp_fgi;
	struct mfi_progress	mlp_reconstruct;
	struct mfi_progress	mlp_res[4];
} __packed;

struct mfi_ld_details {
	struct mfi_ld_cfg	mld_cfg;
	u_quad_t		mld_size;
	struct mfi_ld_progress	mld_progress;
	uint16_t		mld_clust_own_id;
	uint8_t			mld_res1;
	uint8_t			mld_res2;
	uint8_t			mld_inq_page83[64];
	uint8_t			mld_res[16];
} __packed;

/* physical disk info from MR_DCMD_PD_GET_LIST */
struct mfi_pd_address {
	uint16_t		mpa_pd_id;
	uint16_t		mpa_enc_id;
	uint8_t			mpa_enc_index;
	uint8_t			mpa_enc_slot;
	uint8_t			mpa_scsi_type;
	uint8_t			mpa_port;
	u_quad_t		mpa_sas_address[2];
} __packed;

#define MAX_SYS_PDS 240
struct mfi_pd_list {
	uint32_t		mpl_size;
	uint32_t		mpl_no_pd;
	struct mfi_pd_address	mpl_address[MAX_SYS_PDS];
} __packed;
#define MFI_PD_LIST_SIZE	(256 * sizeof(struct mfi_pd_address) + 8)

struct mfi_pd {
	uint16_t		mfp_id;
	uint16_t		mfp_seq;
} __packed;

struct mfi_pd_progress {
	uint32_t		mfp_in_prog;
#define MFI_PD_PROG_RBLD	0x01
#define MFI_PD_PROG_PR		0x02
#define MFI_PD_PROG_CLEAR	0x04
	struct mfi_progress	mfp_rebuild;
	struct mfi_progress	mfp_patrol_read;
	struct mfi_progress	mfp_clear;
	struct mfi_progress	mfp_res[4];
} __packed;

struct mfi_pd_details {
	struct mfi_pd		mpd_pd;
	uint8_t			mpd_inq_data[96];
	uint8_t			mpd_inq_page83[64];
	uint8_t			mpd_no_support;
	uint8_t			mpd_scsy_type;
	uint8_t			mpd_port;
	uint8_t			mpd_speed;
	uint32_t		mpd_mediaerr_cnt;
	uint32_t		mpd_othererr_cnt;
	uint32_t		mpd_predfail_cnt;
	uint32_t		mpd_last_pred_event;
	uint16_t		mpd_fw_state;
	uint8_t			mpd_rdy_for_remove;
	uint8_t			mpd_link_speed;
	uint32_t		mpd_ddf_state;
#define MFI_DDF_GUID_FORCED	0x01
#define MFI_DDF_PART_OF_VD	0x02
#define MFI_DDF_GLOB_HOTSPARE	0x04
#define MFI_DDF_HOTSPARE	0x08
#define MFI_DDF_FOREIGN		0x10
#define MFI_DDF_TYPE_MASK	0xf000
#define MFI_DDF_TYPE_UNKNOWN	0x0000
#define MFI_DDF_TYPE_PAR_SCSI	0x1000
#define MFI_DDF_TYPE_SAS	0x2000
#define MFI_DDF_TYPE_SATA	0x3000
#define MFI_DDF_TYPE_FC		0x4000
	struct {
		uint8_t		mpp_cnt;
		uint8_t		mpp_severed;
		uint8_t		mpp_res[6];
		u_quad_t	mpp_sas_addr[4];
	} __packed mpd_path;
	u_quad_t		mpd_size;
	u_quad_t		mpd_no_coerce_size;
	u_quad_t		mpd_coerce_size;
	uint16_t		mpd_enc_id;
	uint8_t			mpd_enc_idx;
	uint8_t			mpd_enc_slot;
	struct mfi_pd_progress	mpd_progress;
	uint8_t			mpd_bblock_full;
	uint8_t			mpd_unusable;
	uint8_t			mpd_res[218]; /* size is 512 */
} __packed;

/* array configuration from MD_DCMD_CONF_GET */
struct mfi_array {
	u_quad_t		mar_smallest_pd;
	uint8_t			mar_no_disk;
	uint8_t			mar_res1;
	uint16_t		mar_array_ref;
	uint8_t			mar_res2[20];
	struct {
		struct mfi_pd	mar_pd;
		uint16_t	mar_pd_state;
#define MFI_PD_UNCONFIG_GOOD	0x00
#define MFI_PD_UNCONFIG_BAD	0x01
#define MFI_PD_HOTSPARE		0x02
#define MFI_PD_OFFLINE		0x10
#define MFI_PD_FAILED		0x11
#define MFI_PD_REBUILD		0x14
#define MFI_PD_ONLINE		0x18
#define MFI_PD_COPYBACK		0x20
#define MFI_PD_SYSTEM		0x40
#define MFI_PD_JBOD		MFI_PD_SYSTEM
		uint8_t		mar_enc_pd;
		uint8_t		mar_enc_slot;
	} pd[MFI_MAX_PD_ARRAY];
} __packed;

/* informations from MR_DCMD_BBU_GET_CAPACITY_INFO */
struct mfi_bbu_capacity_info {
	uint16_t		relative_charge;
	uint16_t		absolute_charge;
	uint16_t		remaining_capacity;
	uint16_t		full_charge_capacity;
	uint16_t		run_time_to_empty;
	uint16_t		average_time_to_empty;
	uint16_t		average_time_to_full;
	uint16_t		cycle_count;
	uint16_t		max_error;
	uint16_t		remaining_capacity_alarm;
	uint16_t		remaining_time_alarm;
	uint8_t			reserved[26];
} __packed;

/* informations from MR_DCMD_BBU_GET_DESIGN_INFO */
struct mfi_bbu_design_info {
	uint32_t		mfg_date;
	uint16_t		design_capacity;
	uint16_t		design_voltage;
	uint16_t		spec_info;
	uint16_t		serial_number;
	uint16_t		pack_stat_config;
	uint8_t		 	mfg_name[12];
	uint8_t		 	device_name[8];
	uint8_t		 	device_chemistry[8];
	uint8_t		 	mfg_data[8];
	uint8_t		 	reserved[17];
} __packed;

struct mfi_ibbu_state {
	uint16_t		gas_guage_status;
	uint16_t		relative_charge;
	uint16_t		charger_system_state;
	uint16_t		charger_system_ctrl;
	uint16_t		charging_current;
	uint16_t		absolute_charge;
	uint16_t		max_error;
	uint8_t			reserved[18];
} __packed;

struct mfi_bbu_state {
	uint16_t		gas_guage_status;
	uint16_t		relative_charge;
	uint16_t		charger_status;
	uint16_t		remaining_capacity;
	uint16_t		full_charge_capacity;
	uint8_t			is_SOH_good;
	uint8_t			reserved[21];
} __packed;

union mfi_bbu_status_detail {
	struct mfi_ibbu_state	ibbu; 
	struct mfi_bbu_state	bbu;	
};

/* informations from MR_DCMD_BBU_GET_STATUS */
struct mfi_bbu_status {
	uint8_t			battery_type;
#define MFI_BBU_TYPE_NONE	0	
#define MFI_BBU_TYPE_IBBU	1	
#define MFI_BBU_TYPE_BBU	2	
	uint8_t			reserved;
	uint16_t		voltage;
	int16_t			current;
	uint16_t		temperature;
	uint32_t		fw_status;
#define MFI_BBU_STATE_PACK_MISSING	(1 << 0)
#define MFI_BBU_STATE_VOLTAGE_LOW	(1 << 1)
#define MFI_BBU_STATE_TEMPERATURE_HIGH	(1 << 2)
#define MFI_BBU_STATE_CHARGE_ACTIVE	(1 << 3)
#define MFI_BBU_STATE_DISCHARGE_ACTIVE	(1 << 4)
#define MFI_BBU_STATE_LEARN_CYC_REQ	(1 << 5)
#define MFI_BBU_STATE_LEARN_CYC_ACTIVE	(1 << 6)
#define MFI_BBU_STATE_LEARN_CYC_FAIL	(1 << 7)
#define MFI_BBU_STATE_LEARN_CYC_TIMEOUT (1 << 8)
#define MFI_BBU_STATE_I2C_ERR_DETECT	(1 << 9)
	uint8_t			pad[20];
	union mfi_bbu_status_detail detail;
} __packed;

struct mfi_hotspare {
	struct mfi_pd	mhs_pd;
	uint8_t		mhs_type;
#define MFI_PD_HS_DEDICATED	0x01
#define MFI_PD_HS_REVERTIBLE	0x02
#define MFI_PD_HS_ENC_AFFINITY	0x04
	uint8_t		mhs_res[2];
	uint8_t		mhs_array_max;
	uint16_t	mhs_array_ref[MFI_MAX_ARRAY_DEDICATED];
} __packed;

struct mfi_conf {
	uint32_t		mfc_size;
	uint16_t		mfc_no_array;
	uint16_t		mfc_array_size;
	uint16_t		mfc_no_ld;
	uint16_t		mfc_ld_size;
	uint16_t		mfc_no_hs;
	uint16_t		mfc_hs_size;
	uint8_t			mfc_res[16];
	/*
	 * XXX this is a ridiculous hack and does not reflect reality
	 * Structures are actually indexed and therefore need pointer
	 * math to reach.  We need the size of this structure first so
	 * call it with the size of this structure and then use the returned
	 * values to allocate memory and do the transfer of the whole structure
	 * then calculate pointers to each of these structures.
	 */
	struct mfi_array	mfc_array[1];
	struct mfi_ld_cfg	mfc_ld[1];
	struct mfi_hotspare	mfc_hs[1];
} __packed;

/* ThunderBolt support */

/*
 * Raid Context structure which describes MegaRAID specific IO Paramenters
 * This resides at offset 0x60 where the SGL normally starts in MPT IO Frames
 */
typedef struct _mpi2_scsi_io_vendor_unique {
	uint16_t	resvd0;		/* 0x00 - 0x01 */
	uint16_t	timeoutValue;	/* 0x02 - 0x03 */
	uint8_t		regLockFlags;
	uint8_t		armId;
	uint16_t	TargetID;	/* 0x06 - 0x07 */

	uint64_t	RegLockLBA;	/* 0x08 - 0x0F */

	uint32_t	RegLockLength;	/* 0x10 - 0x13 */

	uint16_t	SMID;		/* 0x14 - 0x15 nextLMId */
	uint8_t		exStatus;	/* 0x16 */
	uint8_t		Status;		/* 0x17 status */

	uint8_t		RAIDFlags;	/* 0x18 */
	uint8_t		numSGE;		/* 0x19 numSge */
	uint16_t	configSeqNum;	/* 0x1A - 0x1B */
	uint8_t		spanArm;	/* 0x1C */
	uint8_t		resvd2[3];	/* 0x1D - 0x1F */
} mpi2_scsi_io_vendor_unique, mpi25_scsi_io_vendor_unique;

/*****************************************************************************
*
*        Message Functions
*
*****************************************************************************/

#define NA_MPI2_FUNCTION_SCSI_IO_REQUEST            (0x00) /* SCSI IO */
#define MPI2_FUNCTION_SCSI_TASK_MGMT                (0x01) /* SCSI Task Management */
#define MPI2_FUNCTION_IOC_INIT                      (0x02) /* IOC Init */
#define MPI2_FUNCTION_IOC_FACTS                     (0x03) /* IOC Facts */
#define MPI2_FUNCTION_CONFIG                        (0x04) /* Configuration */
#define MPI2_FUNCTION_PORT_FACTS                    (0x05) /* Port Facts */
#define MPI2_FUNCTION_PORT_ENABLE                   (0x06) /* Port Enable */
#define MPI2_FUNCTION_EVENT_NOTIFICATION            (0x07) /* Event Notification */
#define MPI2_FUNCTION_EVENT_ACK                     (0x08) /* Event Acknowledge */
#define MPI2_FUNCTION_FW_DOWNLOAD                   (0x09) /* FW Download */
#define MPI2_FUNCTION_TARGET_ASSIST                 (0x0B) /* Target Assist */
#define MPI2_FUNCTION_TARGET_STATUS_SEND            (0x0C) /* Target Status Send */
#define MPI2_FUNCTION_TARGET_MODE_ABORT             (0x0D) /* Target Mode Abort */
#define MPI2_FUNCTION_FW_UPLOAD                     (0x12) /* FW Upload */
#define MPI2_FUNCTION_RAID_ACTION                   (0x15) /* RAID Action */
#define MPI2_FUNCTION_RAID_SCSI_IO_PASSTHROUGH      (0x16) /* SCSI IO RAID Passthrough */
#define MPI2_FUNCTION_TOOLBOX                       (0x17) /* Toolbox */
#define MPI2_FUNCTION_SCSI_ENCLOSURE_PROCESSOR      (0x18) /* SCSI Enclosure Processor */
#define MPI2_FUNCTION_SMP_PASSTHROUGH               (0x1A) /* SMP Passthrough */
#define MPI2_FUNCTION_SAS_IO_UNIT_CONTROL           (0x1B) /* SAS IO Unit Control */
#define MPI2_FUNCTION_SATA_PASSTHROUGH              (0x1C) /* SATA Passthrough */
#define MPI2_FUNCTION_DIAG_BUFFER_POST              (0x1D) /* Diagnostic Buffer Post */
#define MPI2_FUNCTION_DIAG_RELEASE                  (0x1E) /* Diagnostic Release */
#define MPI2_FUNCTION_TARGET_CMD_BUF_BASE_POST      (0x24) /* Target Command Buffer Post Base */
#define MPI2_FUNCTION_TARGET_CMD_BUF_LIST_POST      (0x25) /* Target Command Buffer Post List */
#define MPI2_FUNCTION_RAID_ACCELERATOR              (0x2C) /* RAID Accelerator */
#define MPI2_FUNCTION_HOST_BASED_DISCOVERY_ACTION   (0x2F) /* Host Based Discovery Action */
#define MPI2_FUNCTION_PWR_MGMT_CONTROL              (0x30) /* Power Management Control */
#define MPI2_FUNCTION_MIN_PRODUCT_SPECIFIC          (0xF0) /* beginning of product-specific range */
#define MPI2_FUNCTION_MAX_PRODUCT_SPECIFIC          (0xFF) /* end of product-specific range */

/* Doorbell functions */
#define MPI2_FUNCTION_IOC_MESSAGE_UNIT_RESET        (0x40)
#define MPI2_FUNCTION_HANDSHAKE                     (0x42)

/*****************************************************************************
*
*        MPI Version Definitions
*
*****************************************************************************/

#define MPI2_VERSION_MAJOR                  (0x02)
#define MPI2_VERSION_MINOR                  (0x00)
#define MPI2_VERSION_MAJOR_MASK             (0xFF00)
#define MPI2_VERSION_MAJOR_SHIFT            (8)
#define MPI2_VERSION_MINOR_MASK             (0x00FF)
#define MPI2_VERSION_MINOR_SHIFT            (0)
#define MPI2_VERSION ((MPI2_VERSION_MAJOR << MPI2_VERSION_MAJOR_SHIFT) |   \
                                      MPI2_VERSION_MINOR)

#define MPI2_VERSION_02_00                  (0x0200)

/* versioning for this MPI header set */
#define MPI2_HEADER_VERSION_UNIT            (0x10)
#define MPI2_HEADER_VERSION_DEV             (0x00)
#define MPI2_HEADER_VERSION_UNIT_MASK       (0xFF00)
#define MPI2_HEADER_VERSION_UNIT_SHIFT      (8)
#define MPI2_HEADER_VERSION_DEV_MASK        (0x00FF)
#define MPI2_HEADER_VERSION_DEV_SHIFT       (0)
#define MPI2_HEADER_VERSION ((MPI2_HEADER_VERSION_UNIT << 8) |		\
					MPI2_HEADER_VERSION_DEV)


/* IOCInit Request message */
struct mpi2_ioc_init_request {
	uint8_t		WhoInit;                        /* 0x00 */
	uint8_t		Reserved1;                      /* 0x01 */
	uint8_t		ChainOffset;                    /* 0x02 */
	uint8_t		Function;                       /* 0x03 */
	uint16_t	Reserved2;                      /* 0x04 */
	uint8_t		Reserved3;                      /* 0x06 */
	uint8_t		MsgFlags;                       /* 0x07 */
	uint8_t		VP_ID;                          /* 0x08 */
	uint8_t		VF_ID;                          /* 0x09 */
	uint16_t	Reserved4;                      /* 0x0A */
	uint16_t	MsgVersion;                     /* 0x0C */
	uint16_t	HeaderVersion;                  /* 0x0E */
	uint32_t	Reserved5;                      /* 0x10 */
	uint16_t	Reserved6;                      /* 0x14 */
	uint8_t		Reserved7;                      /* 0x16 */
	uint8_t		HostMSIxVectors;                /* 0x17 */
	uint16_t	Reserved8;                      /* 0x18 */
	uint16_t	SystemRequestFrameSize;         /* 0x1A */
	uint16_t	ReplyDescriptorPostQueueDepth;  /* 0x1C */
	uint16_t	ReplyFreeQueueDepth;            /* 0x1E */
	uint32_t	SenseBufferAddressHigh;         /* 0x20 */
	uint32_t	SystemReplyAddressHigh;         /* 0x24 */
	uint64_t	SystemRequestFrameBaseAddress;  /* 0x28 */
	uint64_t	ReplyDescriptorPostQueueAddress;/* 0x30 */
	uint64_t	ReplyFreeQueueAddress;          /* 0x38 */
	uint64_t	TimeStamp;                      /* 0x40 */
};

/* WhoInit values */
#define MPI2_WHOINIT_NOT_INITIALIZED            (0x00)
#define MPI2_WHOINIT_SYSTEM_BIOS                (0x01)
#define MPI2_WHOINIT_ROM_BIOS                   (0x02)
#define MPI2_WHOINIT_PCI_PEER                   (0x03)
#define MPI2_WHOINIT_HOST_DRIVER                (0x04)
#define MPI2_WHOINIT_MANUFACTURER               (0x05)

struct mpi2_sge_chain_union {
	uint16_t	Length;
	uint8_t		NextChainOffset;
	uint8_t		Flags;
	union {
		uint32_t	Address32;
		uint64_t	Address64;
	} u;
};

struct mpi2_ieee_sge_simple32 {
	uint32_t	Address;
	uint32_t	FlagsLength;
};

struct mpi2_ieee_sge_simple64 {
	uint64_t	Address;
	uint32_t	Length;
	uint16_t	Reserved1;
	uint8_t		Reserved2;
	uint8_t		Flags;
};

typedef union _mpi2_ieee_simple_union {
	struct mpi2_ieee_sge_simple32	Simple32;
	struct mpi2_ieee_sge_simple64	Simple64;
} mpi2_ieee_simple_union;

typedef struct _mpi2_sge_simple_union {
	uint32_t	FlagsLength;
	union {
		uint32_t	Address32;
		uint64_t	Address64;
	} u;
} mpi2_sge_simple_union;

/* MPI 2.5 SGLs */

#define MPI25_IEEE_SGE_FLAGS_END_OF_LIST        (0x40)

struct mpi25_ieee_sge_chain64 {
	uint64_t	Address;
	uint32_t	Length;
	uint16_t	Reserved1;
	uint8_t		NextChainOffset;
	uint8_t		Flags;
};

/* use MPI2_IEEE_SGE_FLAGS_ defines for the Flags field */

/****************************************************************************
*  IEEE SGE field definitions and masks
****************************************************************************/

/* Flags field bit definitions */

#define MPI2_IEEE_SGE_FLAGS_ELEMENT_TYPE_MASK   (0x80)

#define MPI2_IEEE32_SGE_FLAGS_SHIFT             (24)

#define MPI2_IEEE32_SGE_LENGTH_MASK             (0x00FFFFFF)

/* Element Type */

#define MPI2_IEEE_SGE_FLAGS_SIMPLE_ELEMENT      (0x00)
#define MPI2_IEEE_SGE_FLAGS_CHAIN_ELEMENT       (0x80)

/* Data Location Address Space */

#define MPI2_IEEE_SGE_FLAGS_ADDR_MASK           (0x03)
#define MPI2_IEEE_SGE_FLAGS_SYSTEM_ADDR         (0x00)
#define MPI2_IEEE_SGE_FLAGS_IOCDDR_ADDR         (0x01)
#define MPI2_IEEE_SGE_FLAGS_IOCPLB_ADDR         (0x02)
#define MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR      (0x03)

/* Address Size */

#define MPI2_SGE_FLAGS_32_BIT_ADDRESSING        (0x00)
#define MPI2_SGE_FLAGS_64_BIT_ADDRESSING        (0x02)

/*******************/
/* SCSI IO Control bits */
#define MPI2_SCSIIO_CONTROL_ADDCDBLEN_MASK      (0xFC000000)
#define MPI2_SCSIIO_CONTROL_ADDCDBLEN_SHIFT     (26)

#define MPI2_SCSIIO_CONTROL_DATADIRECTION_MASK  (0x03000000)
#define MPI2_SCSIIO_CONTROL_NODATATRANSFER      (0x00000000)
#define MPI2_SCSIIO_CONTROL_WRITE               (0x01000000)
#define MPI2_SCSIIO_CONTROL_READ                (0x02000000)
#define MPI2_SCSIIO_CONTROL_BIDIRECTIONAL       (0x03000000)

#define MPI2_SCSIIO_CONTROL_TASKPRI_MASK        (0x00007800)
#define MPI2_SCSIIO_CONTROL_TASKPRI_SHIFT       (11)

#define MPI2_SCSIIO_CONTROL_TASKATTRIBUTE_MASK  (0x00000700)
#define MPI2_SCSIIO_CONTROL_SIMPLEQ             (0x00000000)
#define MPI2_SCSIIO_CONTROL_HEADOFQ             (0x00000100)
#define MPI2_SCSIIO_CONTROL_ORDEREDQ            (0x00000200)
#define MPI2_SCSIIO_CONTROL_ACAQ                (0x00000400)

#define MPI2_SCSIIO_CONTROL_TLR_MASK            (0x000000C0)
#define MPI2_SCSIIO_CONTROL_NO_TLR              (0x00000000)
#define MPI2_SCSIIO_CONTROL_TLR_ON              (0x00000040)
#define MPI2_SCSIIO_CONTROL_TLR_OFF             (0x00000080)

/*******************/

typedef struct {
	uint8_t		CDB[20];                    /* 0x00 */
	uint32_t	PrimaryReferenceTag;        /* 0x14 */
	uint16_t	PrimaryApplicationTag;      /* 0x18 */
	uint16_t	PrimaryApplicationTagMask;  /* 0x1A */
	uint32_t	TransferLength;             /* 0x1C */
} mpi2_scsi_io_cdb_eedp32;


typedef union _mpi2_ieee_sge_chain_union {
	struct mpi2_ieee_sge_simple32	Chain32;
	struct mpi25_ieee_sge_chain64	Chain64;
} mpi2_ieee_sge_chain_union;

typedef union _mpi2_simple_sge_union {
	mpi2_sge_simple_union		MpiSimple;
	mpi2_ieee_simple_union	IeeeSimple;
} mpi2_simple_sge_union;

typedef union _mpi2_sge_io_union {
	mpi2_sge_simple_union		MpiSimple;
	struct mpi2_sge_chain_union	MpiChain;
	mpi2_ieee_simple_union	IeeeSimple;
	mpi2_ieee_sge_chain_union	IeeeChain;
} mpi2_sge_io_union;

typedef union {
	uint8_t			CDB32[32];
	mpi2_scsi_io_cdb_eedp32	EEDP32;
	mpi2_sge_simple_union	SGE;
} mpi2_scsi_io_cdb_union;



/********/

/*
 * RAID SCSI IO Request Message
 * Total SGE count will be one less than  _MPI2_SCSI_IO_REQUEST
 */
struct mfi_mpi2_request_raid_scsi_io {
	uint16_t		DevHandle;                      /* 0x00 */
	uint8_t			ChainOffset;                    /* 0x02 */
	uint8_t			Function;                       /* 0x03 */
	uint16_t		Reserved1;                      /* 0x04 */
	uint8_t			Reserved2;                      /* 0x06 */
	uint8_t			MsgFlags;                       /* 0x07 */
	uint8_t			VP_ID;                          /* 0x08 */
	uint8_t			VF_ID;                          /* 0x09 */
	uint16_t		Reserved3;                      /* 0x0A */
	uint32_t		SenseBufferLowAddress;          /* 0x0C */
	uint16_t		SGLFlags;                       /* 0x10 */
	uint8_t			SenseBufferLength;              /* 0x12 */
	uint8_t			Reserved4;                      /* 0x13 */
	uint8_t			SGLOffset0;                     /* 0x14 */
	uint8_t			SGLOffset1;                     /* 0x15 */
	uint8_t			SGLOffset2;                     /* 0x16 */
	uint8_t			SGLOffset3;                     /* 0x17 */
	uint32_t		SkipCount;                      /* 0x18 */
	uint32_t		DataLength;                     /* 0x1C */
	uint32_t		BidirectionalDataLength;        /* 0x20 */
	uint16_t		IoFlags;                        /* 0x24 */
	uint16_t		EEDPFlags;                      /* 0x26 */
	uint32_t		EEDPBlockSize;                  /* 0x28 */
	uint32_t		SecondaryReferenceTag;          /* 0x2C */
	uint16_t		SecondaryApplicationTag;        /* 0x30 */
	uint16_t		ApplicationTagTranslationMask;  /* 0x32 */
	uint8_t			LUN[8];                         /* 0x34 */
	uint32_t		Control;                        /* 0x3C */
	mpi2_scsi_io_cdb_union	CDB;                            /* 0x40 */
	mpi2_scsi_io_vendor_unique	RaidContext;              /* 0x60 */
	mpi2_sge_io_union	SGL;                            /* 0x80 */
} __packed;

/*
 * MPT RAID MFA IO Descriptor.
 */
typedef struct _mfi_raid_mfa_io_descriptor {
	uint32_t	RequestFlags : 8;
	uint32_t	MessageAddress1 : 24; /* bits 31:8*/
	uint32_t	MessageAddress2;      /* bits 61:32 */
} mfi_raid_mfa_io_request_descriptor;

struct mfi_mpi2_request_header {
	uint8_t		RequestFlags;       /* 0x00 */
	uint8_t		MSIxIndex;          /* 0x01 */
	uint16_t	SMID;               /* 0x02 */
	uint16_t	LMID;               /* 0x04 */
};

/* defines for the RequestFlags field */
#define MPI2_REQ_DESCRIPT_FLAGS_TYPE_MASK               (0x0E)
#define MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO                 (0x00)
#define MPI2_REQ_DESCRIPT_FLAGS_SCSI_TARGET             (0x02)
#define MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY           (0x06)
#define MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE            (0x08)
#define MPI2_REQ_DESCRIPT_FLAGS_RAID_ACCELERATOR        (0x0A)

#define MPI2_REQ_DESCRIPT_FLAGS_IOC_FIFO_MARKER (0x01)

struct mfi_mpi2_request_high_priority {
	struct mfi_mpi2_request_header	header;
	uint16_t			reserved;
};

struct mfi_mpi2_request_scsi_io {
	struct mfi_mpi2_request_header	header;
	uint16_t			scsi_io_dev_handle;
};

struct mfi_mpi2_request_scsi_target {
	struct mfi_mpi2_request_header	header;
	uint16_t			scsi_target_io_index;
};

/* Request Descriptors */
union mfi_mpi2_request_descriptor {
	struct mfi_mpi2_request_header		header;
	struct mfi_mpi2_request_high_priority	high_priority;
	struct mfi_mpi2_request_scsi_io		scsi_io;
	struct mfi_mpi2_request_scsi_target	scsi_target;
	uint64_t				words;
};

/* 
 * Request descriptor types
 */
#define MFI_REQ_DESCRIPT_FLAGS_LD_IO		0x7
#define MFI_REQ_DESCRIPT_FLAGS_MFA		0x1
#define MFI_REQ_DESCRIPT_FLAGS_TYPE_SHIFT	0x1

#define MFI_FUSION_FP_DEFAULT_TIMEOUT		0x14

struct mfi_mpi2_reply_header {
	uint8_t		ReplyFlags;                 /* 0x00 */
	uint8_t		MSIxIndex;                  /* 0x01 */
	uint16_t	SMID;                       /* 0x02 */
};

/* defines for the ReplyFlags field */
#define MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK                   (0x0F)
#define MPI2_RPY_DESCRIPT_FLAGS_SCSI_IO_SUCCESS             (0x00)
#define MPI2_RPY_DESCRIPT_FLAGS_ADDRESS_REPLY               (0x01)
#define MPI2_RPY_DESCRIPT_FLAGS_TARGETASSIST_SUCCESS        (0x02)
#define MPI2_RPY_DESCRIPT_FLAGS_TARGET_COMMAND_BUFFER       (0x03)
#define MPI2_RPY_DESCRIPT_FLAGS_RAID_ACCELERATOR_SUCCESS    (0x05)
#define MPI2_RPY_DESCRIPT_FLAGS_UNUSED                      (0x0F)

/* values for marking a reply descriptor as unused */
#define MPI2_RPY_DESCRIPT_UNUSED_WORD0_MARK             (0xFFFFFFFF)
#define MPI2_RPY_DESCRIPT_UNUSED_WORD1_MARK             (0xFFFFFFFF)

struct mfi_mpi2_reply_default {
	struct mfi_mpi2_reply_header	header;
	uint32_t			DescriptorTypeDependent2;
};

struct mfi_mpi2_reply_address {
	struct mfi_mpi2_reply_header	header;
	uint32_t			ReplyFrameAddress;
};

struct mfi_mpi2_reply_scsi_io {
	struct mfi_mpi2_reply_header	header;
	uint16_t			TaskTag;		/* 0x04 */
	uint16_t			Reserved1;		/* 0x06 */
};

struct mfi_mpi2_reply_target_assist {
	struct mfi_mpi2_reply_header	header;
	uint8_t				SequenceNumber;		/* 0x04 */
	uint8_t				Reserved1;		/* 0x04 */
	uint16_t			IoIndex;		/* 0x06 */
};

struct mfi_mpi2_reply_target_cmd_buffer {
	struct mfi_mpi2_reply_header	header;
	uint8_t				SequenceNumber;		/* 0x04 */
	uint8_t				Flags;			/* 0x04 */
	uint16_t			InitiatorDevHandle;	/* 0x06 */
	uint16_t			IoIndex;		/* 0x06 */
};

struct mfi_mpi2_reply_raid_accel {
	struct mfi_mpi2_reply_header	header;
	uint8_t				SequenceNumber;		/* 0x04 */
	uint32_t			Reserved;		/* 0x04 */
};

/* union of Reply Descriptors */
union mfi_mpi2_reply_descriptor {
	struct mfi_mpi2_reply_header		header;
	struct mfi_mpi2_reply_scsi_io		scsi_io;
	struct mfi_mpi2_reply_target_assist	target_assist;
	struct mfi_mpi2_reply_target_cmd_buffer	target_cmd;
	struct mfi_mpi2_reply_raid_accel	raid_accel;
	struct mfi_mpi2_reply_default		reply_default;
	uint64_t				words;
};

struct io_request_info {
	uint64_t	ldStartBlock;
	uint32_t	numBlocks;
	uint16_t	ldTgtId;
	uint8_t		isRead;
	uint16_t	devHandle;
	uint64_t	pdBlock;
	uint8_t		fpOkForIo;
};

/*    
 * Define MFI Address Context union. 
 */   
#ifdef MFI_ADDRESS_IS_uint64_t
typedef uint64_t     mfi_address;
#else
typedef union _mfi_address {
	struct {
		uint32_t     addressLow;
		uint32_t     addressHigh;
	} u;
	uint64_t     address;
} mfi_address;
#endif

#define MEGASAS_MAX_NAME        32
#define MEGASAS_VERSION         "4.23"

#endif /* _DEV_IC_MFIREG_H_ */
