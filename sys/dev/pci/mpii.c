/* $NetBSD: mpii.c,v 1.6 2015/03/12 15:33:10 christos Exp $ */
/*	OpenBSD: mpii.c,v 1.51 2012/04/11 13:29:14 naddy Exp 	*/
/*
 * Copyright (c) 2010 Mike Belopuhov <mkb@crypt.org.ru>
 * Copyright (c) 2009 James Giannoules
 * Copyright (c) 2005 - 2010 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2005 - 2010 Marco Peereboom <marco@openbsd.org>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mpii.c,v 1.6 2015/03/12 15:33:10 christos Exp $");

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/dkio.h>
#include <sys/tree.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/biovar.h>
#include <dev/sysmon/sysmonvar.h>
#include <sys/envsys.h>

#define MPII_DOORBELL			(0x00)
/* doorbell read bits */
#define MPII_DOORBELL_STATE		(0xf<<28) /* ioc state */
#define  MPII_DOORBELL_STATE_RESET	(0x0<<28)
#define  MPII_DOORBELL_STATE_READY	(0x1<<28)
#define  MPII_DOORBELL_STATE_OPER	(0x2<<28)
#define  MPII_DOORBELL_STATE_FAULT	(0x4<<28)
#define  MPII_DOORBELL_INUSE		(0x1<<27) /* doorbell used */
#define MPII_DOORBELL_WHOINIT		(0x7<<24) /* last to reset ioc */
#define  MPII_DOORBELL_WHOINIT_NOONE	(0x0<<24) /* not initialized */
#define  MPII_DOORBELL_WHOINIT_SYSBIOS	(0x1<<24) /* system bios */
#define  MPII_DOORBELL_WHOINIT_ROMBIOS	(0x2<<24) /* rom bios */
#define  MPII_DOORBELL_WHOINIT_PCIPEER	(0x3<<24) /* pci peer */
#define  MPII_DOORBELL_WHOINIT_DRIVER	(0x4<<24) /* host driver */
#define  MPII_DOORBELL_WHOINIT_MANUFACT	(0x5<<24) /* manufacturing */
#define MPII_DOORBELL_FAULT		(0xffff<<0) /* fault code */
/* doorbell write bits */
#define MPII_DOORBELL_FUNCTION_SHIFT	(24)
#define MPII_DOORBELL_FUNCTION_MASK	(0xff << MPII_DOORBELL_FUNCTION_SHIFT)
#define MPII_DOORBELL_FUNCTION(x)	\
    (((x) << MPII_DOORBELL_FUNCTION_SHIFT) & MPII_DOORBELL_FUNCTION_MASK)
#define MPII_DOORBELL_DWORDS_SHIFT	16
#define MPII_DOORBELL_DWORDS_MASK	(0xff << MPII_DOORBELL_DWORDS_SHIFT)
#define MPII_DOORBELL_DWORDS(x)		\
    (((x) << MPII_DOORBELL_DWORDS_SHIFT) & MPII_DOORBELL_DWORDS_MASK)
#define MPII_DOORBELL_DATA_MASK		(0xffff)

#define MPII_WRITESEQ			(0x04)
#define  MPII_WRITESEQ_KEY_VALUE_MASK	(0x0000000f) /* key value */
#define  MPII_WRITESEQ_FLUSH		(0x00)
#define  MPII_WRITESEQ_1		(0x0f)
#define  MPII_WRITESEQ_2		(0x04)
#define  MPII_WRITESEQ_3		(0x0b)
#define  MPII_WRITESEQ_4		(0x02)
#define  MPII_WRITESEQ_5		(0x07)
#define  MPII_WRITESEQ_6		(0x0d)

#define MPII_HOSTDIAG			(0x08)
#define  MPII_HOSTDIAG_BDS_MASK		(0x00001800) /* boot device select */
#define   MPII_HOSTDIAG_BDS_DEFAULT 	(0<<11)	/* default address map, flash */
#define   MPII_HOSTDIAG_BDS_HCDW	(1<<11)	/* host code and data window */
#define  MPII_HOSTDIAG_CLEARFBS		(1<<10) /* clear flash bad sig */
#define  MPII_HOSTDIAG_FORCE_HCB_ONBOOT (1<<9)	/* force host controlled boot */
#define  MPII_HOSTDIAG_HCB_MODE		(1<<8)	/* host controlled boot mode */
#define  MPII_HOSTDIAG_DWRE		(1<<7) 	/* diag reg write enabled */
#define  MPII_HOSTDIAG_FBS		(1<<6) 	/* flash bad sig */
#define  MPII_HOSTDIAG_RESET_HIST	(1<<5) 	/* reset history */
#define  MPII_HOSTDIAG_DIAGWR_EN	(1<<4) 	/* diagnostic write enabled */
#define  MPII_HOSTDIAG_RESET_ADAPTER	(1<<2) 	/* reset adapter */
#define  MPII_HOSTDIAG_HOLD_IOC_RESET	(1<<1) 	/* hold ioc in reset */
#define  MPII_HOSTDIAG_DIAGMEM_EN	(1<<0) 	/* diag mem enable */

#define MPII_DIAGRWDATA			(0x10)

#define MPII_DIAGRWADDRLOW		(0x14)

#define MPII_DIAGRWADDRHIGH		(0x18)

#define MPII_INTR_STATUS		(0x30)
#define  MPII_INTR_STATUS_SYS2IOCDB	(1<<31) /* ioc written to by host */
#define  MPII_INTR_STATUS_RESET		(1<<30) /* physical ioc reset */
#define  MPII_INTR_STATUS_REPLY		(1<<3)	/* reply message interrupt */
#define  MPII_INTR_STATUS_IOC2SYSDB	(1<<0) 	/* ioc write to doorbell */

#define MPII_INTR_MASK			(0x34)
#define  MPII_INTR_MASK_RESET		(1<<30) /* ioc reset intr mask */
#define  MPII_INTR_MASK_REPLY		(1<<3) 	/* reply message intr mask */
#define  MPII_INTR_MASK_DOORBELL	(1<<0) 	/* doorbell interrupt mask */

#define MPII_DCR_DATA			(0x38)

#define MPII_DCR_ADDRESS		(0x3c)

#define MPII_REPLY_FREE_HOST_INDEX	(0x48)

#define MPII_REPLY_POST_HOST_INDEX	(0x6c)

#define MPII_HCB_SIZE			(0x74)

#define MPII_HCB_ADDRESS_LOW		(0x78)
#define MPII_HCB_ADDRESS_HIGH		(0x7c)

#define MPII_REQ_DESCR_POST_LOW		(0xc0)
#define MPII_REQ_DESCR_POST_HIGH	(0xc4)

/*
 * Scatter Gather Lists
 */

#define MPII_SGE_FL_LAST		(0x1<<31) /* last element in segment */
#define MPII_SGE_FL_EOB			(0x1<<30) /* last element of buffer */
#define MPII_SGE_FL_TYPE		(0x3<<28) /* element type */
 #define MPII_SGE_FL_TYPE_SIMPLE	(0x1<<28) /* simple element */
 #define MPII_SGE_FL_TYPE_CHAIN		(0x3<<28) /* chain element */
 #define MPII_SGE_FL_TYPE_XACTCTX	(0x0<<28) /* transaction context */
#define MPII_SGE_FL_LOCAL		(0x1<<27) /* local address */
#define MPII_SGE_FL_DIR			(0x1<<26) /* direction */
 #define MPII_SGE_FL_DIR_OUT		(0x1<<26)
 #define MPII_SGE_FL_DIR_IN		(0x0<<26)
#define MPII_SGE_FL_SIZE		(0x1<<25) /* address size */
 #define MPII_SGE_FL_SIZE_32		(0x0<<25)
 #define MPII_SGE_FL_SIZE_64		(0x1<<25)
#define MPII_SGE_FL_EOL			(0x1<<24) /* end of list */

struct mpii_sge {
	u_int32_t		sg_hdr;
	u_int32_t		sg_lo_addr;
	u_int32_t		sg_hi_addr;
} __packed;

struct mpii_fw_tce {
	u_int8_t		reserved1;
	u_int8_t		context_size;
	u_int8_t		details_length;
	u_int8_t		flags;

	u_int32_t		reserved2;

	u_int32_t		image_offset;

	u_int32_t		image_size;
} __packed;

/*
 * Messages
 */

/* functions */
#define MPII_FUNCTION_SCSI_IO_REQUEST			(0x00)
#define MPII_FUNCTION_SCSI_TASK_MGMT			(0x01)
#define MPII_FUNCTION_IOC_INIT				(0x02)
#define MPII_FUNCTION_IOC_FACTS				(0x03)
#define MPII_FUNCTION_CONFIG				(0x04)
#define MPII_FUNCTION_PORT_FACTS			(0x05)
#define MPII_FUNCTION_PORT_ENABLE			(0x06)
#define MPII_FUNCTION_EVENT_NOTIFICATION		(0x07)
#define MPII_FUNCTION_EVENT_ACK				(0x08)
#define MPII_FUNCTION_FW_DOWNLOAD			(0x09)
#define MPII_FUNCTION_TARGET_CMD_BUFFER_POST		(0x0a)
#define MPII_FUNCTION_TARGET_ASSIST			(0x0b)
#define MPII_FUNCTION_TARGET_STATUS_SEND		(0x0c)
#define MPII_FUNCTION_TARGET_MODE_ABORT			(0x0d)
#define MPII_FUNCTION_FW_UPLOAD				(0x12)

#define MPII_FUNCTION_RAID_ACTION			(0x15)
#define MPII_FUNCTION_RAID_SCSI_IO_PASSTHROUGH		(0x16)

#define MPII_FUNCTION_TOOLBOX				(0x17)

#define MPII_FUNCTION_SCSI_ENCLOSURE_PROCESSOR		(0x18)

#define MPII_FUNCTION_SMP_PASSTHROUGH			(0x1a)
#define MPII_FUNCTION_SAS_IO_UNIT_CONTROL		(0x1b)
#define MPII_FUNCTION_SATA_PASSTHROUGH			(0x1c)

#define MPII_FUNCTION_DIAG_BUFFER_POST			(0x1d)
#define MPII_FUNCTION_DIAG_RELEASE			(0x1e)

#define MPII_FUNCTION_TARGET_CMD_BUF_BASE_POST		(0x24)
#define MPII_FUNCTION_TARGET_CMD_BUF_LIST_POST		(0x25)

#define MPII_FUNCTION_IOC_MESSAGE_UNIT_RESET		(0x40)
#define MPII_FUNCTION_IO_UNIT_RESET			(0x41)
#define MPII_FUNCTION_HANDSHAKE				(0x42)

/* Common IOCStatus values for all replies */
#define MPII_IOCSTATUS_MASK				(0x7fff)
#define  MPII_IOCSTATUS_SUCCESS				(0x0000)
#define  MPII_IOCSTATUS_INVALID_FUNCTION		(0x0001)
#define  MPII_IOCSTATUS_BUSY				(0x0002)
#define  MPII_IOCSTATUS_INVALID_SGL			(0x0003)
#define  MPII_IOCSTATUS_INTERNAL_ERROR			(0x0004)
#define  MPII_IOCSTATUS_INVALID_VPID			(0x0005)
#define  MPII_IOCSTATUS_INSUFFICIENT_RESOURCES		(0x0006)
#define  MPII_IOCSTATUS_INVALID_FIELD			(0x0007)
#define  MPII_IOCSTATUS_INVALID_STATE			(0x0008)
#define  MPII_IOCSTATUS_OP_STATE_NOT_SUPPORTED		(0x0009)
/* Config IOCStatus values */
#define  MPII_IOCSTATUS_CONFIG_INVALID_ACTION		(0x0020)
#define  MPII_IOCSTATUS_CONFIG_INVALID_TYPE		(0x0021)
#define  MPII_IOCSTATUS_CONFIG_INVALID_PAGE		(0x0022)
#define  MPII_IOCSTATUS_CONFIG_INVALID_DATA		(0x0023)
#define  MPII_IOCSTATUS_CONFIG_NO_DEFAULTS		(0x0024)
#define  MPII_IOCSTATUS_CONFIG_CANT_COMMIT		(0x0025)
/* SCSIIO Reply initiator values */
#define  MPII_IOCSTATUS_SCSI_RECOVERED_ERROR		(0x0040)
#define  MPII_IOCSTATUS_SCSI_INVALID_DEVHANDLE		(0x0042)
#define  MPII_IOCSTATUS_SCSI_DEVICE_NOT_THERE		(0x0043)
#define  MPII_IOCSTATUS_SCSI_DATA_OVERRUN		(0x0044)
#define  MPII_IOCSTATUS_SCSI_DATA_UNDERRUN		(0x0045)
#define  MPII_IOCSTATUS_SCSI_IO_DATA_ERROR		(0x0046)
#define  MPII_IOCSTATUS_SCSI_PROTOCOL_ERROR		(0x0047)
#define  MPII_IOCSTATUS_SCSI_TASK_TERMINATED		(0x0048)
#define  MPII_IOCSTATUS_SCSI_RESIDUAL_MISMATCH		(0x0049)
#define  MPII_IOCSTATUS_SCSI_TASK_MGMT_FAILED		(0x004a)
#define  MPII_IOCSTATUS_SCSI_IOC_TERMINATED		(0x004b)
#define  MPII_IOCSTATUS_SCSI_EXT_TERMINATED		(0x004c)
/* For use by SCSI Initiator and SCSI Target end-to-end data protection */
#define  MPII_IOCSTATUS_EEDP_GUARD_ERROR		(0x004d)
#define  MPII_IOCSTATUS_EEDP_REF_TAG_ERROR		(0x004e)
#define  MPII_IOCSTATUS_EEDP_APP_TAG_ERROR		(0x004f)
/* SCSI (SPI & FCP) target values */
#define  MPII_IOCSTATUS_TARGET_INVALID_IO_INDEX		(0x0062)
#define  MPII_IOCSTATUS_TARGET_ABORTED			(0x0063)
#define  MPII_IOCSTATUS_TARGET_NO_CONN_RETRYABLE	(0x0064)
#define  MPII_IOCSTATUS_TARGET_NO_CONNECTION		(0x0065)
#define  MPII_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH	(0x006a)
#define  MPII_IOCSTATUS_TARGET_DATA_OFFSET_ERROR	(0x006d)
#define  MPII_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA	(0x006e)
#define  MPII_IOCSTATUS_TARGET_IU_TOO_SHORT		(0x006f)
#define  MPII_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT		(0x0070)
#define  MPII_IOCSTATUS_TARGET_NAK_RECEIVED		(0x0071)
/* Serial Attached SCSI values */
#define  MPII_IOCSTATUS_SAS_SMP_REQUEST_FAILED		(0x0090)
#define  MPII_IOCSTATUS_SAS_SMP_DATA_OVERRUN		(0x0091)
/* Diagnostic Tools values */
#define  MPII_IOCSTATUS_DIAGNOSTIC_RELEASED		(0x00a0)

#define MPII_REP_IOCLOGINFO_TYPE			(0xf<<28)
#define MPII_REP_IOCLOGINFO_TYPE_NONE			(0x0<<28)
#define MPII_REP_IOCLOGINFO_TYPE_SCSI			(0x1<<28)
#define MPII_REP_IOCLOGINFO_TYPE_FC			(0x2<<28)
#define MPII_REP_IOCLOGINFO_TYPE_SAS			(0x3<<28)
#define MPII_REP_IOCLOGINFO_TYPE_ISCSI			(0x4<<28)
#define MPII_REP_IOCLOGINFO_DATA			(0x0fffffff)

/* event notification types */
#define MPII_EVENT_NONE					(0x00)
#define MPII_EVENT_LOG_DATA				(0x01)
#define MPII_EVENT_STATE_CHANGE				(0x02)
#define MPII_EVENT_HARD_RESET_RECEIVED			(0x05)
#define MPII_EVENT_EVENT_CHANGE				(0x0a)
#define MPII_EVENT_TASK_SET_FULL			(0x0e)
#define MPII_EVENT_SAS_DEVICE_STATUS_CHANGE		(0x0f)
#define MPII_EVENT_IR_OPERATION_STATUS			(0x14)
#define MPII_EVENT_SAS_DISCOVERY			(0x16)
#define MPII_EVENT_SAS_BROADCAST_PRIMITIVE		(0x17)
#define MPII_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE	(0x18)
#define MPII_EVENT_SAS_INIT_TABLE_OVERFLOW		(0x19)
#define MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST		(0x1c)
#define MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE	(0x1d)
#define MPII_EVENT_IR_VOLUME				(0x1e)
#define MPII_EVENT_IR_PHYSICAL_DISK			(0x1f)
#define MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST		(0x20)
#define MPII_EVENT_LOG_ENTRY_ADDED			(0x21)

/* messages */

#define MPII_WHOINIT_NOONE				(0x00)
#define MPII_WHOINIT_SYSTEM_BIOS			(0x01)
#define MPII_WHOINIT_ROM_BIOS				(0x02)
#define MPII_WHOINIT_PCI_PEER				(0x03)
#define MPII_WHOINIT_HOST_DRIVER			(0x04)
#define MPII_WHOINIT_MANUFACTURER			(0x05)

/* default messages */

struct mpii_msg_request {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		reserved5;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved6;
} __packed;

struct mpii_msg_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_if;
	u_int16_t		reserved4;
	
	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

/* ioc init */

struct mpii_msg_iocinit_request {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;
	u_int8_t		hdr_version_unit;
	u_int8_t		hdr_version_dev;

	u_int32_t		reserved5;

	u_int32_t		reserved6;

	u_int16_t		reserved7;
	u_int16_t		system_request_frame_size;

	u_int16_t		reply_descriptor_post_queue_depth;
	u_int16_t		reply_free_queue_depth;

	u_int32_t		sense_buffer_address_high;

	u_int32_t		system_reply_address_high;

	u_int64_t		system_request_frame_base_address;

	u_int64_t		reply_descriptor_post_queue_address;

	u_int64_t		reply_free_queue_address;

	u_int64_t		timestamp;
} __packed;

struct mpii_msg_iocinit_reply {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_iocfacts_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;
} __packed;

struct mpii_msg_iocfacts_reply {
	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		header_version_dev;
	u_int8_t		header_version_unit;
	u_int8_t		ioc_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int16_t		ioc_exceptions;
#define MPII_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL	(1<<0)
#define MPII_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID	(1<<1)
#define MPII_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL		(1<<2)
#define MPII_IOCFACTS_EXCEPT_MANUFACT_CHECKSUM_FAIL	(1<<3)
#define MPII_IOCFACTS_EXCEPT_METADATA_UNSUPPORTED	(1<<4)
#define MPII_IOCFACTS_EXCEPT_IR_FOREIGN_CONFIG_MAC	(1<<8)
	/* XXX JPG BOOT_STATUS in bits[7:5] */
	/* XXX JPG all these #defines need to be fixed up */
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		max_chain_depth;
	u_int8_t		whoinit;
	u_int8_t		number_of_ports;
	u_int8_t		reserved2;

	u_int16_t		request_credit;
	u_int16_t		product_id;

	u_int32_t		ioc_capabilities;
#define MPII_IOCFACTS_CAPABILITY_EVENT_REPLAY           (1<<13)
#define MPII_IOCFACTS_CAPABILITY_INTEGRATED_RAID        (1<<12)
#define MPII_IOCFACTS_CAPABILITY_TLR                    (1<<11)
#define MPII_IOCFACTS_CAPABILITY_MULTICAST              (1<<8)
#define MPII_IOCFACTS_CAPABILITY_BIDIRECTIONAL_TARGET   (1<<7)
#define MPII_IOCFACTS_CAPABILITY_EEDP                   (1<<6)
#define MPII_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER        (1<<4)
#define MPII_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER      (1<<3)
#define MPII_IOCFACTS_CAPABILITY_TASK_SET_FULL_HANDLING (1<<2)

	u_int8_t		fw_version_dev;
	u_int8_t		fw_version_unit;
	u_int8_t		fw_version_min;
	u_int8_t		fw_version_maj;

	u_int16_t		ioc_request_frame_size;
	u_int16_t		reserved3;

	u_int16_t		max_initiators;
	u_int16_t		max_targets;

	u_int16_t		max_sas_expanders;
	u_int16_t		max_enclosures;

	u_int16_t		protocol_flags;
	u_int16_t		high_priority_credit;

	u_int16_t		max_reply_descriptor_post_queue_depth;
	u_int8_t		reply_frame_size;
	u_int8_t		max_volumes;

	u_int16_t		max_dev_handle;
	u_int16_t		max_persistent_entries;

	u_int32_t		reserved4;
} __packed;

struct mpii_msg_portfacts_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;
} __packed;

struct mpii_msg_portfacts_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		reserved5;
	u_int8_t		port_type;
#define MPII_PORTFACTS_PORTTYPE_INACTIVE		(0x00)
#define MPII_PORTFACTS_PORTTYPE_FC			(0x10)
#define MPII_PORTFACTS_PORTTYPE_ISCSI			(0x20)
#define MPII_PORTFACTS_PORTTYPE_SAS_PHYSICAL		(0x30)
#define MPII_PORTFACTS_PORTTYPE_SAS_VIRTUAL		(0x31)
	u_int16_t		reserved6;

	u_int16_t		max_posted_cmd_buffers;
	u_int16_t		reserved7;
} __packed;

struct mpii_msg_portenable_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2;
	u_int8_t		port_flags;	
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;
} __packed;

struct mpii_msg_portenable_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2;
	u_int8_t		port_flags;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_event_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int32_t		reserved5;

	u_int32_t		reserved6;

	u_int32_t		event_masks[4];
	
	u_int16_t		sas_broadcase_primitive_masks;
	u_int16_t		reserved7;

	u_int32_t		reserved8;
} __packed;

struct mpii_msg_event_reply {
	u_int16_t		event_data_length;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		ack_required;
#define MPII_EVENT_ACK_REQUIRED				(0x01)
	u_int8_t		msg_flags;
#define MPII_EVENT_FLAGS_REPLY_KEPT			(1<<7)

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved2;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int16_t		event;
	u_int16_t		reserved4;

	u_int32_t		event_context;

	/* event data follows */
} __packed;

struct mpii_msg_eventack_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		event;
	u_int16_t		reserved4;

	u_int32_t		event_context;
} __packed;

struct mpii_msg_eventack_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_fwupload_request {
	u_int8_t		image_type;
#define MPII_FWUPLOAD_IMAGETYPE_IOC_FW			(0x00)
#define MPII_FWUPLOAD_IMAGETYPE_NV_FW			(0x01)
#define MPII_FWUPLOAD_IMAGETYPE_NV_BACKUP		(0x05)
#define MPII_FWUPLOAD_IMAGETYPE_NV_MANUFACTURING	(0x06)
#define MPII_FWUPLOAD_IMAGETYPE_NV_CONFIG_1		(0x07)
#define MPII_FWUPLOAD_IMAGETYPE_NV_CONFIG_2		(0x08)
#define MPII_FWUPLOAD_IMAGETYPE_NV_MEGARAID		(0x09)
#define MPII_FWUPLOAD_IMAGETYPE_NV_COMPLETE		(0x0a)
#define MPII_FWUPLOAD_IMAGETYPE_COMMON_BOOT_BLOCK	(0x0b)
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int32_t		reserved4;

	u_int32_t		reserved5;

	struct mpii_fw_tce	tce;

	/* followed by an sgl */
} __packed;

struct mpii_msg_fwupload_reply {
	u_int8_t		image_type;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		actual_image_size;
} __packed;

struct mpii_msg_scsi_io {
	u_int16_t		dev_handle;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;
	
	u_int32_t		sense_buffer_low_address;

	u_int16_t		sgl_flags;
	u_int8_t		sense_buffer_length;
	u_int8_t		reserved4;

	u_int8_t		sgl_offset0;
	u_int8_t		sgl_offset1;
	u_int8_t		sgl_offset2;
	u_int8_t		sgl_offset3;

	u_int32_t		skip_count;

	u_int32_t		data_length;

	u_int32_t		bidirectional_data_length;

	u_int16_t		io_flags;
	u_int16_t		eedp_flags;

	u_int32_t		eedp_block_size;

	u_int32_t		secondary_reference_tag;

	u_int16_t		secondary_application_tag;
	u_int16_t		application_tag_translation_mask;

	u_int16_t		lun[4];

/* the following 16 bits are defined in MPI2 as the control field */
	u_int8_t		reserved5;
	u_int8_t		tagging;
#define MPII_SCSIIO_ATTR_SIMPLE_Q			(0x0)
#define MPII_SCSIIO_ATTR_HEAD_OF_Q			(0x1)
#define MPII_SCSIIO_ATTR_ORDERED_Q			(0x2)
#define MPII_SCSIIO_ATTR_ACA_Q				(0x4)
#define MPII_SCSIIO_ATTR_UNTAGGED			(0x5)
#define MPII_SCSIIO_ATTR_NO_DISCONNECT			(0x7)
	u_int8_t		reserved6;
	u_int8_t		direction;
#define MPII_SCSIIO_DIR_NONE				(0x0)
#define MPII_SCSIIO_DIR_WRITE				(0x1)
#define MPII_SCSIIO_DIR_READ				(0x2)

#define	MPII_CDB_LEN					(32)
	u_int8_t		cdb[MPII_CDB_LEN];

	/* followed by an sgl */
} __packed;

struct mpii_msg_scsi_io_error {
	u_int16_t		dev_handle;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int8_t		scsi_status;

#define MPII_SCSIIO_ERR_STATUS_SUCCESS			(0x00)
#define MPII_SCSIIO_ERR_STATUS_CHECK_COND		(0x02)
#define MPII_SCSIIO_ERR_STATUS_BUSY			(0x04)
#define MPII_SCSIIO_ERR_STATUS_INTERMEDIATE		(0x08)
#define MPII_SCSIIO_ERR_STATUS_INTERMEDIATE_CONDMET	(0x10)
#define MPII_SCSIIO_ERR_STATUS_RESERVATION_CONFLICT	(0x14)
#define MPII_SCSIIO_ERR_STATUS_CMD_TERM			(0x22)
#define MPII_SCSIIO_ERR_STATUS_TASK_SET_FULL		(0x28)
#define MPII_SCSIIO_ERR_STATUS_ACA_ACTIVE		(0x30)
#define MPII_SCSIIO_ERR_STATUS_TASK_ABORTED		(0x40)

	u_int8_t		scsi_state;
#define MPII_SCSIIO_ERR_STATE_AUTOSENSE_VALID		(1<<0)
#define MPII_SCSIIO_ERR_STATE_AUTOSENSE_FAILED		(1<<1)
#define MPII_SCSIIO_ERR_STATE_NO_SCSI_STATUS		(1<<2)
#define MPII_SCSIIO_ERR_STATE_TERMINATED		(1<<3)
#define MPII_SCSIIO_ERR_STATE_RESPONSE_INFO_VALID	(1<<4)
#define MPII_SCSIIO_ERR_STATE_QUEUE_TAG_REJECTED	(0xffff)
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		transfer_count;

	u_int32_t		sense_count;

	u_int32_t		response_info;

	u_int16_t		task_tag;
	u_int16_t		reserved4;

	u_int32_t		bidirectional_transfer_count;

	u_int32_t		reserved5;

	u_int32_t		reserved6;
} __packed;

struct mpii_request_descr {
	u_int8_t		request_flags;
#define MPII_REQ_DESCR_TYPE_MASK			(0x0e)
#define MPII_REQ_DESCR_SCSI_IO				(0x00)
#define MPII_REQ_DESCR_SCSI_TARGET			(0x02)
#define MPII_REQ_DESCR_HIGH_PRIORITY			(0x06)
#define MPII_REQ_DESCR_DEFAULT				(0x08)
	u_int8_t		vf_id;
	u_int16_t		smid;

	u_int16_t		lmid;
	u_int16_t		dev_handle;
} __packed;

struct mpii_reply_descr {
	u_int8_t		reply_flags;
#define MPII_REPLY_DESCR_TYPE_MASK               	(0x0f)
#define MPII_REPLY_DESCR_SCSI_IO_SUCCESS         	(0x00)
#define MPII_REPLY_DESCR_ADDRESS_REPLY           	(0x01)
#define MPII_REPLY_DESCR_TARGET_ASSIST_SUCCESS    	(0x02)
#define MPII_REPLY_DESCR_TARGET_COMMAND_BUFFER   	(0x03)
#define MPII_REPLY_DESCR_UNUSED                  	(0x0f)
	u_int8_t		vf_id;
	u_int16_t		smid;

	union {
		u_int32_t	data;
		u_int32_t	frame_addr;	/* Address Reply */
	};
} __packed;

struct mpii_request_header {
	u_int16_t		function_dependent1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		function_dependent2;
	u_int8_t		function_dependent3;
	u_int8_t		message_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved;
} __packed;

struct mpii_msg_scsi_task_request {
	u_int16_t		dev_handle;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved1;
	u_int8_t		task_type;
#define MPII_SCSI_TASK_ABORT_TASK			(0x01)
#define MPII_SCSI_TASK_ABRT_TASK_SET			(0x02)
#define MPII_SCSI_TASK_TARGET_RESET			(0x03)
#define MPII_SCSI_TASK_RESET_BUS			(0x04)
#define MPII_SCSI_TASK_LOGICAL_UNIT_RESET		(0x05)
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		lun[4];

	u_int32_t		reserved4[7];

	u_int16_t		task_mid;
	u_int16_t		reserved5;
} __packed;

struct mpii_msg_scsi_task_reply {
	u_int16_t		dev_handle;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		response_code;
	u_int8_t		task_type;
	u_int8_t		reserved1;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved2;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		termination_count;
} __packed;

struct mpii_msg_sas_oper_request {
	u_int8_t		operation;
#define MPII_SAS_OP_CLEAR_PERSISTENT		(0x02)
#define MPII_SAS_OP_PHY_LINK_RESET		(0x06)
#define MPII_SAS_OP_PHY_HARD_RESET		(0x07)
#define MPII_SAS_OP_PHY_CLEAR_ERROR_LOG		(0x08)
#define MPII_SAS_OP_SEND_PRIMITIVE		(0x0a)
#define MPII_SAS_OP_FORCE_FULL_DISCOVERY	(0x0b)
#define MPII_SAS_OP_TRANSMIT_PORT_SELECT	(0x0c)
#define MPII_SAS_OP_REMOVE_DEVICE		(0x0d)
#define MPII_SAS_OP_LOOKUP_MAPPING		(0x0e)
#define MPII_SAS_OP_SET_IOC_PARAM		(0x0f)
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		dev_handle;
	u_int8_t		ioc_param;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved2;

	u_int16_t		reserved3;
	u_int8_t		phy_num;
	u_int8_t		prim_flags;

	u_int32_t		primitive;

	u_int8_t		lookup_method;
#define MPII_SAS_LOOKUP_METHOD_SAS_ADDR		(0x01)
#define MPII_SAS_LOOKUP_METHOD_SAS_ENCL		(0x02)
#define MPII_SAS_LOOKUP_METHOD_SAS_DEVNAME	(0x03)
	u_int8_t		reserved4;
	u_int16_t		slot_num;

	u_int64_t		lookup_addr;

	u_int32_t		ioc_param_value;

	u_int64_t		reserved5;
} __packed;

struct mpii_msg_sas_oper_reply {
	u_int8_t		operation;
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		dev_handle;
	u_int8_t		ioc_param;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved2;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_raid_action_request {
	u_int8_t	action;
#define MPII_RAID_ACTION_CHANGE_VOL_WRITE_CACHE	(0x17)
	u_int8_t	reserved1;
	u_int8_t	chain_offset;
	u_int8_t	function;

	u_int16_t	vol_dev_handle;
	u_int8_t	phys_disk_num;
	u_int8_t	msg_flags;

	u_int8_t	vp_id;
	u_int8_t	vf_if;
	u_int16_t	reserved2;

	u_int32_t	reserved3;

	u_int32_t	action_data;
#define MPII_RAID_VOL_WRITE_CACHE_MASK			(0x03)
#define MPII_RAID_VOL_WRITE_CACHE_DISABLE		(0x01)
#define MPII_RAID_VOL_WRITE_CACHE_ENABLE		(0x02)

	struct mpii_sge	action_sge;
} __packed;

struct mpii_msg_raid_action_reply {
	u_int8_t	action;
	u_int8_t	reserved1;
	u_int8_t	chain_offset;
	u_int8_t	function;

	u_int16_t	vol_dev_handle;
	u_int8_t	phys_disk_num;
	u_int8_t	msg_flags;

	u_int8_t	vp_id;
	u_int8_t	vf_if;
	u_int16_t	reserved2;

	u_int16_t	reserved3;
	u_int16_t	ioc_status;

	u_int32_t	action_data[5];
} __packed;

struct mpii_cfg_hdr {
	u_int8_t		page_version;
	u_int8_t		page_length;
	u_int8_t		page_number;
	u_int8_t		page_type;
#define MPII_CONFIG_REQ_PAGE_TYPE_ATTRIBUTE		(0xf0)
#define MPI2_CONFIG_PAGEATTR_READ_ONLY              	(0x00)
#define MPI2_CONFIG_PAGEATTR_CHANGEABLE             	(0x10)
#define MPI2_CONFIG_PAGEATTR_PERSISTENT             	(0x20)

#define MPII_CONFIG_REQ_PAGE_TYPE_MASK			(0x0f)
#define MPII_CONFIG_REQ_PAGE_TYPE_IO_UNIT		(0x00)
#define MPII_CONFIG_REQ_PAGE_TYPE_IOC			(0x01)
#define MPII_CONFIG_REQ_PAGE_TYPE_BIOS			(0x02)
#define MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL		(0x08)
#define MPII_CONFIG_REQ_PAGE_TYPE_MANUFACTURING		(0x09)
#define MPII_CONFIG_REQ_PAGE_TYPE_RAID_PD		(0x0a)
#define MPII_CONFIG_REQ_PAGE_TYPE_EXTENDED		(0x0f)
} __packed;

struct mpii_ecfg_hdr {
	u_int8_t		page_version;
	u_int8_t		reserved1;
	u_int8_t		page_number;
	u_int8_t		page_type;

	u_int16_t		ext_page_length;
	u_int8_t		ext_page_type;
#define MPII_CONFIG_REQ_PAGE_TYPE_SAS_DEVICE		(0x12)
#define MPII_CONFIG_REQ_PAGE_TYPE_RAID_CONFIG		(0x16)
#define MPII_CONFIG_REQ_PAGE_TYPE_DRIVER_MAPPING	(0x17)
	u_int8_t		reserved2;
} __packed;

struct mpii_msg_config_request {
	u_int8_t		action;
#define MPII_CONFIG_REQ_ACTION_PAGE_HEADER		(0x00)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_CURRENT	(0x01)
#define MPII_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT	(0x02)
#define MPII_CONFIG_REQ_ACTION_PAGE_DEFAULT		(0x03)
#define MPII_CONFIG_REQ_ACTION_PAGE_WRITE_NVRAM		(0x04)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_DEFAULT	(0x05)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_NVRAM		(0x06)
	u_int8_t		sgl_flags;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		ext_page_len;
	u_int8_t		ext_page_type;
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_IO_UNIT	(0x10)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_EXPANDER	(0x11)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_DEVICE		(0x12)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_PHY		(0x13)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_LOG		(0x14)
#define MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE            	(0x15)
#define MPI2_CONFIG_EXTPAGETYPE_RAID_CONFIG         	(0x16)
#define MPI2_CONFIG_EXTPAGETYPE_DRIVER_MAPPING      	(0x17)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_PORT            	(0x18)
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int32_t		reserved2[2];

	struct mpii_cfg_hdr	config_header;

	u_int32_t		page_address;
/* XXX lots of defns here */

	struct mpii_sge		page_buffer;
} __packed;

struct mpii_msg_config_reply {
	u_int8_t		action;
	u_int8_t		sgl_flags;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		ext_page_length;
	u_int8_t		ext_page_type;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	struct mpii_cfg_hdr	config_header;
} __packed;

struct mpii_cfg_manufacturing_pg0 {
	struct mpii_cfg_hdr	config_header;

	char			chip_name[16];
	char			chip_revision[8];
	char			board_name[16];
	char			board_assembly[16];
	char			board_tracer_number[16];
} __packed;

struct mpii_cfg_ioc_pg1 {
	struct mpii_cfg_hdr     config_header;

	u_int32_t       flags;

	u_int32_t       coalescing_timeout;
#define	MPII_CFG_IOC_1_REPLY_COALESCING			(1<<0)

	u_int8_t        coalescing_depth;
	u_int8_t        pci_slot_num;
	u_int8_t        pci_bus_num;
	u_int8_t        pci_domain_segment;

	u_int32_t       reserved1;

	u_int32_t       reserved2;
} __packed;

struct mpii_cfg_ioc_pg3 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		no_phys_disks;
	u_int8_t		reserved[3];

	/* followed by a list of mpii_cfg_raid_physdisk structs */
} __packed;

struct mpii_cfg_ioc_pg8 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		num_devs_per_enclosure;
	u_int8_t		reserved1;
	u_int16_t		reserved2;

	u_int16_t		max_persistent_entries;
	u_int16_t		max_num_physical_mapped_ids;

	u_int16_t		flags;
#define	MPII_IOC_PG8_FLAGS_DA_START_SLOT_1		(1<<5)
#define MPII_IOC_PG8_FLAGS_RESERVED_TARGETID_0		(1<<4)
#define MPII_IOC_PG8_FLAGS_MAPPING_MODE_MASK		(0x0000000e)
#define MPII_IOC_PG8_FLAGS_DEVICE_PERSISTENCE_MAPPING	(0<<1)
#define MPII_IOC_PG8_FLAGS_ENCLOSURE_SLOT_MAPPING	(1<<1)
#define MPII_IOC_PG8_FLAGS_DISABLE_PERSISTENT_MAPPING	(1<<0)
#define	MPII_IOC_PG8_FLAGS_ENABLE_PERSISTENT_MAPPING	(0<<0)
	u_int16_t		reserved3;

	u_int16_t		ir_volume_mapping_flags;
#define	MPII_IOC_PG8_IRFLAGS_VOLUME_MAPPING_MODE_MASK	(0x00000003)
#define	MPII_IOC_PG8_IRFLAGS_LOW_VOLUME_MAPPING		(0<<0)
#define	MPII_IOC_PG8_IRFLAGS_HIGH_VOLUME_MAPPING	(1<<0)
	u_int16_t		reserved4;
	
	u_int32_t		reserved5;
} __packed;

struct mpii_cfg_raid_physdisk {
	u_int8_t		phys_disk_id;
	u_int8_t		phys_disk_bus;
	u_int8_t		phys_disk_ioc;
	u_int8_t		phys_disk_num;
} __packed;

struct mpii_cfg_fc_port_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int32_t		flags;

	u_int8_t		mpii_port_nr;
	u_int8_t		link_type;
	u_int8_t		port_state;
	u_int8_t		reserved1;

	u_int32_t		port_id;

	u_int64_t		wwnn;

	u_int64_t		wwpn;

	u_int32_t		supported_service_class;

	u_int32_t		supported_speeds;

	u_int32_t		current_speed;

	u_int32_t		max_frame_size;

	u_int64_t		fabric_wwnn;

	u_int64_t		fabric_wwpn;

	u_int32_t		discovered_port_count;

	u_int32_t		max_initiators;

	u_int8_t		max_aliases_supported;
	u_int8_t		max_hard_aliases_supported;
	u_int8_t		num_current_aliases;
	u_int8_t		reserved2;
} __packed;

struct mpii_cfg_fc_port_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int32_t		flags;

	u_int64_t		noseepromwwnn;

	u_int64_t		noseepromwwpn;

	u_int8_t		hard_alpa;
	u_int8_t		link_config;
	u_int8_t		topology_config;
	u_int8_t		alt_connector;

	u_int8_t		num_req_aliases;
	u_int8_t		rr_tov;
	u_int8_t		initiator_dev_to;
	u_int8_t		initiator_lo_pend_to;
} __packed;

struct mpii_cfg_fc_device_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int64_t		wwnn;

	u_int64_t		wwpn;

	u_int32_t		port_id;

	u_int8_t		protocol;
	u_int8_t		flags;
	u_int16_t		bb_credit;

	u_int16_t		max_rx_frame_size;
	u_int8_t		adisc_hard_alpa;
	u_int8_t		port_nr;

	u_int8_t		fc_ph_low_version;
	u_int8_t		fc_ph_high_version;
	u_int8_t		current_target_id;
	u_int8_t		current_bus;
} __packed;

#define MPII_CFG_RAID_VOL_ADDR_HANDLE		(1<<28)

struct mpii_cfg_raid_vol_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int16_t		volume_handle;
	u_int8_t		volume_state;
#define MPII_CFG_RAID_VOL_0_STATE_MISSING		(0x00)
#define MPII_CFG_RAID_VOL_0_STATE_FAILED		(0x01)
#define MPII_CFG_RAID_VOL_0_STATE_INITIALIZING		(0x02)
#define MPII_CFG_RAID_VOL_0_STATE_ONLINE		(0x03)
#define MPII_CFG_RAID_VOL_0_STATE_DEGRADED		(0x04)
#define MPII_CFG_RAID_VOL_0_STATE_OPTIMAL		(0x05)
	u_int8_t		volume_type;
#define MPII_CFG_RAID_VOL_0_TYPE_RAID0			(0x00)
#define MPII_CFG_RAID_VOL_0_TYPE_RAID1E			(0x01)
#define MPII_CFG_RAID_VOL_0_TYPE_RAID1			(0x02)
#define MPII_CFG_RAID_VOL_0_TYPE_RAID10			(0x05)
#define MPII_CFG_RAID_VOL_0_TYPE_UNKNOWN		(0xff)

	u_int32_t		volume_status;
#define MPII_CFG_RAID_VOL_0_STATUS_SCRUB		(1<<20)
#define MPII_CFG_RAID_VOL_0_STATUS_RESYNC		(1<<16)

	u_int16_t		volume_settings;
#define MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_MASK		(0x3<<0)
#define MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_UNCHANGED	(0x0<<0)
#define MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_DISABLED	(0x1<<0)
#define MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_ENABLED	(0x2<<0)

	u_int8_t		hot_spare_pool;
	u_int8_t		reserved1;

	u_int64_t		max_lba;

	u_int32_t		stripe_size;

	u_int16_t		block_size;
	u_int16_t		reserved2;

	u_int8_t		phys_disk_types;
	u_int8_t		resync_rate;
	u_int16_t		data_scrub_rate;

	u_int8_t		num_phys_disks;
	u_int16_t		reserved3;
	u_int8_t		inactive_status;
#define MPII_CFG_RAID_VOL_0_INACTIVE_UNKNOWN		(0x00)
#define MPII_CFG_RAID_VOL_0_INACTIVE_STALE_META		(0x01)
#define MPII_CFG_RAID_VOL_0_INACTIVE_FOREIGN_VOL	(0x02)
#define MPII_CFG_RAID_VOL_0_INACTIVE_NO_RESOURCES	(0x03)
#define MPII_CFG_RAID_VOL_0_INACTIVE_CLONED_VOL		(0x04)
#define MPII_CFG_RAID_VOL_0_INACTIVE_INSUF_META		(0x05)

	/* followed by a list of mpii_cfg_raid_vol_pg0_physdisk structs */
} __packed;

struct mpii_cfg_raid_vol_pg0_physdisk {
	u_int8_t		raid_set_num;
	u_int8_t		phys_disk_map;
	u_int8_t		phys_disk_num;
	u_int8_t		reserved;
} __packed;

struct mpii_cfg_raid_vol_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		volume_id;
	u_int8_t		volume_bus;
	u_int8_t		volume_ioc;
	u_int8_t		reserved1;

	u_int8_t		guid[24];

	u_int8_t		name[32];

	u_int64_t		wwid;

	u_int32_t		reserved2;

	u_int32_t		reserved3;
} __packed;

#define MPII_CFG_RAID_PHYS_DISK_ADDR_NUMBER		(1<<28)

struct mpii_cfg_raid_physdisk_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int16_t		dev_handle;
	u_int8_t		reserved1;
	u_int8_t		phys_disk_num;

	u_int8_t		enc_id;
	u_int8_t		enc_bus;
	u_int8_t		hot_spare_pool;
	u_int8_t		enc_type;
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_NONE		(0x0)
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_SAFTE		(0x1)
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_SES		(0x2)

	u_int32_t		reserved2;

	u_int8_t		vendor_id[8];

	u_int8_t		product_id[16];

	u_int8_t		product_rev[4];

	u_int8_t		serial[32];

	u_int32_t		reserved3;

	u_int8_t		phys_disk_state;
#define MPII_CFG_RAID_PHYDISK_0_STATE_NOTCONFIGURED	(0x00)
#define MPII_CFG_RAID_PHYDISK_0_STATE_NOTCOMPATIBLE	(0x01)
#define MPII_CFG_RAID_PHYDISK_0_STATE_OFFLINE		(0x02)
#define MPII_CFG_RAID_PHYDISK_0_STATE_ONLINE		(0x03)
#define MPII_CFG_RAID_PHYDISK_0_STATE_HOTSPARE		(0x04)
#define MPII_CFG_RAID_PHYDISK_0_STATE_DEGRADED		(0x05)
#define MPII_CFG_RAID_PHYDISK_0_STATE_REBUILDING	(0x06)
#define MPII_CFG_RAID_PHYDISK_0_STATE_OPTIMAL		(0x07)
	u_int8_t		offline_reason;
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_MISSING		(0x01)
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_FAILED		(0x03)
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_INITIALIZING	(0x04)
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_REQUESTED	(0x05)
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_FAILEDREQ	(0x06)
#define MPII_CFG_RAID_PHYDISK_0_OFFLINE_OTHER		(0xff)

	u_int8_t		incompat_reason;
	u_int8_t		phys_disk_attrs;

	u_int32_t		phys_disk_status;
#define MPII_CFG_RAID_PHYDISK_0_STATUS_OUTOFSYNC	(1<<0)
#define MPII_CFG_RAID_PHYDISK_0_STATUS_QUIESCED		(1<<1)

	u_int64_t		dev_max_lba;

	u_int64_t		host_max_lba;

	u_int64_t		coerced_max_lba;

	u_int16_t		block_size;
	u_int16_t		reserved4;

	u_int32_t		reserved5;
} __packed;

struct mpii_cfg_raid_physdisk_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		num_phys_disk_paths;
	u_int8_t		phys_disk_num;
	u_int16_t		reserved1;

	u_int32_t		reserved2;

	/* followed by mpii_cfg_raid_physdisk_path structs */
} __packed;

struct mpii_cfg_raid_physdisk_path {
	u_int8_t		phys_disk_id;
	u_int8_t		phys_disk_bus;
	u_int16_t		reserved1;

	u_int64_t		wwwid;

	u_int64_t		owner_wwid;

	u_int8_t		ownder_id;
	u_int8_t		reserved2;
	u_int16_t		flags;
#define MPII_CFG_RAID_PHYDISK_PATH_INVALID	(1<<0)
#define MPII_CFG_RAID_PHYDISK_PATH_BROKEN	(1<<1)
} __packed;

#define MPII_CFG_SAS_DEV_ADDR_NEXT		(0<<28)
#define MPII_CFG_SAS_DEV_ADDR_BUS		(1<<28)
#define MPII_CFG_SAS_DEV_ADDR_HANDLE		(2<<28)

struct mpii_cfg_sas_dev_pg0 {
	struct mpii_ecfg_hdr	config_header;

	u_int16_t		slot;
	u_int16_t		enc_handle;

	u_int64_t		sas_addr;

	u_int16_t		parent_dev_handle;
	u_int8_t		phy_num;
	u_int8_t		access_status;

	u_int16_t		dev_handle;
	u_int8_t		target;
	u_int8_t		bus;

	u_int32_t		device_info;
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE			(0x7)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_NONE		(0x0)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_END		(0x1)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_EDGE_EXPANDER	(0x2)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_FANOUT_EXPANDER	(0x3)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SATA_HOST		(1<<3)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SMP_INITIATOR	(1<<4)
#define MPII_CFG_SAS_DEV_0_DEVINFO_STP_INITIATOR	(1<<5)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SSP_INITIATOR	(1<<6)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SATA_DEVICE		(1<<7)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SMP_TARGET		(1<<8)
#define MPII_CFG_SAS_DEV_0_DEVINFO_STP_TARGET		(1<<9)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SSP_TARGET		(1<<10)
#define MPII_CFG_SAS_DEV_0_DEVINFO_DIRECT_ATTACHED	(1<<11)
#define MPII_CFG_SAS_DEV_0_DEVINFO_LSI_DEVICE		(1<<12)
#define MPII_CFG_SAS_DEV_0_DEVINFO_ATAPI_DEVICE		(1<<13)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SEP_DEVICE		(1<<14)

	u_int16_t		flags;
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_PRESENT		(1<<0)
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_MAPPED		(1<<1)
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_MAPPED_PERSISTENT	(1<<2)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_PORT_SELECTOR	(1<<3)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_FUA		(1<<4)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_NCQ		(1<<5)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_SMART		(1<<6)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_LBA48		(1<<7)
#define MPII_CFG_SAS_DEV_0_FLAGS_UNSUPPORTED		(1<<8)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_SETTINGS		(1<<9)
	u_int8_t		physical_port;
	u_int8_t		max_port_conn;

	u_int64_t		device_name;

	u_int8_t		port_groups;
	u_int8_t		dma_group;
	u_int8_t		ctrl_group;
	u_int8_t		reserved1;

	u_int64_t		reserved2;
} __packed;

#define MPII_CFG_RAID_CONFIG_ACTIVE_CONFIG		(2<<28)

struct mpii_cfg_raid_config_pg0 {
	struct	mpii_ecfg_hdr	config_header;

	u_int8_t		num_hot_spares;
	u_int8_t		num_phys_disks;
	u_int8_t		num_volumes;
	u_int8_t		config_num;

	u_int32_t		flags;
#define MPII_CFG_RAID_CONFIG_0_FLAGS_NATIVE		(0<<0)
#define MPII_CFG_RAID_CONFIG_0_FLAGS_FOREIGN		(1<<0)

	u_int32_t		config_guid[6];

	u_int32_t		reserved1;

	u_int8_t		num_elements;
	u_int8_t		reserved2[3];

	/* followed by struct mpii_raid_config_element structs */
} __packed;

struct mpii_raid_config_element {
	u_int16_t		element_flags;
#define MPII_RAID_CONFIG_ELEMENT_FLAG_VOLUME		(0x0)
#define MPII_RAID_CONFIG_ELEMENT_FLAG_VOLUME_PHYS_DISK	(0x1)
#define	MPII_RAID_CONFIG_ELEMENT_FLAG_HSP_PHYS_DISK	(0x2)
#define MPII_RAID_CONFIG_ELEMENT_ONLINE_CE_PHYS_DISK	(0x3)
	u_int16_t		vol_dev_handle;

	u_int8_t		hot_spare_pool;
	u_int8_t		phys_disk_num;
	u_int16_t		phys_disk_dev_handle;
} __packed;

struct mpii_cfg_dpm_pg0 {
	struct mpii_ecfg_hdr	config_header;
#define MPII_DPM_ADDRESS_FORM_MASK			(0xf0000000)
#define MPII_DPM_ADDRESS_FORM_ENTRY_RANGE		(0x00000000)
#define MPII_DPM_ADDRESS_ENTRY_COUNT_MASK		(0x0fff0000)
#define MPII_DPM_ADDRESS_ENTRY_COUNT_SHIFT		(16)
#define MPII_DPM_ADDRESS_START_ENTRY_MASK		(0x0000ffff)

	/* followed by struct mpii_dpm_entry structs */
} __packed;

struct mpii_dpm_entry {
	u_int64_t		physical_identifier;

	u_int16_t		mapping_information;
	u_int16_t		device_index;

	u_int32_t		physical_bits_mapping;

	u_int32_t		reserved1;
} __packed;

struct mpii_evt_sas_discovery {
	u_int8_t		flags;
#define	MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_MASK	(1<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_NO_CHANGE	(0<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_CHANGE	(1<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROG_MASK	(1<<0)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_NOT_IN_PROGRESS	(1<<0)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROGRESS	(0<<0)
	u_int8_t		reason_code;
#define MPII_EVENT_SAS_DISC_REASON_CODE_STARTED		(0x01)
#define	MPII_EVENT_SAS_DISC_REASON_CODE_COMPLETED	(0x02)
	u_int8_t		physical_port;
	u_int8_t		reserved1;

	u_int32_t		discovery_status;
} __packed;

struct mpii_evt_ir_status {
	u_int16_t		vol_dev_handle;
	u_int16_t		reserved1;

	u_int8_t		operation;
#define MPII_EVENT_IR_RAIDOP_RESYNC			(0x00)
#define MPII_EVENT_IR_RAIDOP_OCE			(0x01)
#define MPII_EVENT_IR_RAIDOP_CONS_CHECK			(0x02)
#define MPII_EVENT_IR_RAIDOP_BG_INIT			(0x03)
#define MPII_EVENT_IR_RAIDOP_MAKE_CONS			(0x04)
	u_int8_t		percent;
	u_int16_t		reserved2;

	u_int32_t		reserved3;
};

struct mpii_evt_ir_volume {
	u_int16_t		vol_dev_handle;
	u_int8_t		reason_code;
#define MPII_EVENT_IR_VOL_RC_SETTINGS_CHANGED		(0x01)
#define MPII_EVENT_IR_VOL_RC_STATUS_CHANGED		(0x02)
#define MPII_EVENT_IR_VOL_RC_STATE_CHANGED		(0x03)
	u_int8_t		reserved1;

	u_int32_t		new_value;
	u_int32_t		prev_value;
} __packed;

struct mpii_evt_ir_physical_disk {
	u_int16_t		reserved1;
	u_int8_t		reason_code;
#define MPII_EVENT_IR_PD_RC_SETTINGS_CHANGED		(0x01)
#define MPII_EVENT_IR_PD_RC_STATUS_FLAGS_CHANGED	(0x02)
#define MPII_EVENT_IR_PD_RC_STATUS_CHANGED		(0x03)
	u_int8_t		phys_disk_num;

	u_int16_t		phys_disk_dev_handle;
	u_int16_t		reserved2;

	u_int16_t		slot;
	u_int16_t		enclosure_handle;

	u_int32_t		new_value;
	u_int32_t		previous_value;
} __packed;

struct mpii_evt_sas_tcl {
	u_int16_t		enclosure_handle;
	u_int16_t		expander_handle;

	u_int8_t		num_phys;
	u_int8_t		reserved1[3];

	u_int8_t		num_entries;
	u_int8_t		start_phy_num;
	u_int8_t		expn_status;
#define	MPII_EVENT_SAS_TOPO_ES_ADDED			(0x01)
#define MPII_EVENT_SAS_TOPO_ES_NOT_RESPONDING		(0x02)
#define MPII_EVENT_SAS_TOPO_ES_RESPONDING		(0x03)
#define MPII_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING	(0x04)
	u_int8_t		physical_port;

	/* followed by num_entries number of struct mpii_evt_phy_entry */
} __packed;

struct mpii_evt_phy_entry {
	u_int16_t		dev_handle;
	u_int8_t		link_rate;
	u_int8_t		phy_status;
#define MPII_EVENT_SAS_TOPO_PS_RC_MASK			(0x0f)
#define MPII_EVENT_SAS_TOPO_PS_RC_ADDED			(0x01)
#define MPII_EVENT_SAS_TOPO_PS_RC_MISSING		(0x02)
} __packed;

struct mpii_evt_ir_cfg_change_list {
	u_int8_t		num_elements;
	u_int16_t		reserved;
	u_int8_t		config_num;

	u_int32_t		flags;
#define MPII_EVT_IR_CFG_CHANGE_LIST_FOREIGN		(0x1)

	/* followed by num_elements struct mpii_evt_ir_cfg_elements */
} __packed;

struct mpii_evt_ir_cfg_element {
	u_int16_t		element_flags;
#define MPII_EVT_IR_CFG_ELEMENT_TYPE_MASK		(0xf)
#define MPII_EVT_IR_CFG_ELEMENT_TYPE_VOLUME		(0x0)
#define MPII_EVT_IR_CFG_ELEMENT_TYPE_VOLUME_DISK	(0x1)
#define MPII_EVT_IR_CFG_ELEMENT_TYPE_HOT_SPARE		(0x2)
	u_int16_t		vol_dev_handle;

	u_int8_t		reason_code;
#define MPII_EVT_IR_CFG_ELEMENT_RC_ADDED		(0x01)
#define MPII_EVT_IR_CFG_ELEMENT_RC_REMOVED		(0x02)
#define MPII_EVT_IR_CFG_ELEMENT_RC_NO_CHANGE		(0x03)
#define MPII_EVT_IR_CFG_ELEMENT_RC_HIDE			(0x04)
#define MPII_EVT_IR_CFG_ELEMENT_RC_UNHIDE		(0x05)
#define MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_CREATED	(0x06)
#define MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_DELETED	(0x07)
#define MPII_EVT_IR_CFG_ELEMENT_RC_PD_CREATED		(0x08)
#define MPII_EVT_IR_CFG_ELEMENT_RC_PD_DELETED		(0x09)
	u_int8_t		phys_disk_num;
	u_int16_t		phys_disk_dev_handle;
} __packed;

/* #define MPII_DEBUG */
#ifdef MPII_DEBUG
#define DPRINTF(x...)		do { if (mpii_debug) printf(x); } while(0)
#define DNPRINTF(n,x...)	do { if (mpii_debug & (n)) printf(x); } while(0)
#define	MPII_D_CMD		(0x0001)
#define	MPII_D_INTR		(0x0002)
#define	MPII_D_MISC		(0x0004)
#define	MPII_D_DMA		(0x0008)
#define	MPII_D_IOCTL		(0x0010)
#define	MPII_D_RW		(0x0020)
#define	MPII_D_MEM		(0x0040)
#define	MPII_D_CCB		(0x0080)
#define	MPII_D_PPR		(0x0100)
#define	MPII_D_RAID		(0x0200)
#define	MPII_D_EVT		(0x0400)
#define MPII_D_CFG		(0x0800)
#define MPII_D_MAP		(0x1000)

#if 0
u_int32_t  mpii_debug = 0
		| MPII_D_CMD
		| MPII_D_INTR
		| MPII_D_MISC
		| MPII_D_DMA
		| MPII_D_IOCTL
		| MPII_D_RW
		| MPII_D_MEM
		| MPII_D_CCB
		| MPII_D_PPR
		| MPII_D_RAID
		| MPII_D_EVT
		| MPII_D_CFG
		| MPII_D_MAP
	;
#endif
u_int32_t  mpii_debug = MPII_D_MISC;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define MPII_REQUEST_SIZE	(512)
#define MPII_REPLY_SIZE		(128)
#define MPII_REPLY_COUNT	PAGE_SIZE / MPII_REPLY_SIZE

/*
 * this is the max number of sge's we can stuff in a request frame:
 * sizeof(scsi_io) + sizeof(sense) + sizeof(sge) * 32 = MPII_REQUEST_SIZE
 */
#define MPII_MAX_SGL			(32)

#define MPII_MAX_REQUEST_CREDIT		(128)

#define MPII_MAXFER MAXPHYS /* XXX bogus */

struct mpii_dmamem {
	bus_dmamap_t		mdm_map;
	bus_dma_segment_t	mdm_seg;
	size_t			mdm_size;
	void 			*mdm_kva;
};
#define MPII_DMA_MAP(_mdm)	(_mdm)->mdm_map
#define MPII_DMA_DVA(_mdm)	(_mdm)->mdm_map->dm_segs[0].ds_addr
#define MPII_DMA_KVA(_mdm)	(void *)(_mdm)->mdm_kva

struct mpii_ccb_bundle {
	struct mpii_msg_scsi_io	mcb_io; /* sgl must follow */
	struct mpii_sge		mcb_sgl[MPII_MAX_SGL];
	struct scsi_sense_data	mcb_sense;
} __packed;

struct mpii_softc;

struct mpii_rcb {
	union {
		struct work	rcb_wk; /* has to be first in struct */
		SIMPLEQ_ENTRY(mpii_rcb)	rcb_link;
	} u;
	void			*rcb_reply;
	u_int32_t		rcb_reply_dva;
};

SIMPLEQ_HEAD(mpii_rcb_list, mpii_rcb);

struct mpii_device {
	int			flags;
#define MPII_DF_ATTACH		(0x0001)
#define MPII_DF_DETACH		(0x0002)
#define MPII_DF_HIDDEN		(0x0004)
#define MPII_DF_UNUSED		(0x0008)
#define MPII_DF_VOLUME		(0x0010)
#define MPII_DF_VOLUME_DISK	(0x0020)
#define MPII_DF_HOT_SPARE	(0x0040)
	short			slot;
	short			percent;
	u_int16_t		dev_handle;
	u_int16_t		enclosure;
	u_int16_t		expander;
	u_int8_t		phy_num;
	u_int8_t		physical_port;
};

struct mpii_ccb {
	union {
		struct work	ccb_wk; /* has to be first in struct */
		SIMPLEQ_ENTRY(mpii_ccb)	ccb_link;
	} u;
	struct mpii_softc	*ccb_sc;
	int			ccb_smid;

	void *			ccb_cookie;
	bus_dmamap_t		ccb_dmamap;

	bus_addr_t		ccb_offset;
	void			*ccb_cmd;
	bus_addr_t		ccb_cmd_dva;
	u_int16_t		ccb_dev_handle;

	volatile enum {
		MPII_CCB_FREE,
		MPII_CCB_READY,
		MPII_CCB_QUEUED,
		MPII_CCB_TIMEOUT
	}			ccb_state;

	void			(*ccb_done)(struct mpii_ccb *);
	struct mpii_rcb		*ccb_rcb;

};

struct mpii_ccb_wait {
	kmutex_t	mpii_ccbw_mtx;
	kcondvar_t	mpii_ccbw_cv;
};

SIMPLEQ_HEAD(mpii_ccb_list, mpii_ccb);

struct mpii_softc {
	device_t		sc_dev;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	void			*sc_ih;

	int			sc_flags;
#define MPII_F_RAID		(1<<1)

	struct scsipi_adapter	sc_adapt;
	struct scsipi_channel	sc_chan;
	device_t		sc_child; /* our scsibus */

	struct mpii_device	**sc_devs;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	kmutex_t		sc_req_mtx;
	kmutex_t		sc_rep_mtx;

	u_int8_t		sc_porttype;
	int			sc_request_depth;
	int			sc_num_reply_frames;
	int			sc_reply_free_qdepth;
	int			sc_reply_post_qdepth;
	int			sc_maxchdepth;
	int			sc_first_sgl_len;
	int			sc_chain_len;
	int			sc_max_sgl_len;

	u_int8_t		sc_ioc_event_replay;
	u_int16_t		sc_max_enclosures;
	u_int16_t		sc_max_expanders;
	u_int8_t		sc_max_volumes;
	u_int16_t		sc_max_devices;
	u_int16_t		sc_max_dpm_entries;
	u_int16_t		sc_vd_count;
	u_int16_t		sc_vd_id_low;
	u_int16_t		sc_pd_id_start;
	u_int8_t		sc_num_channels;
	int			sc_ioc_number;
	u_int8_t		sc_vf_id;
	u_int8_t		sc_num_ports;

	struct mpii_ccb		*sc_ccbs;
	struct mpii_ccb_list	sc_ccb_free;
	kmutex_t		sc_ccb_free_mtx;
	kcondvar_t		sc_ccb_free_cv;

	kmutex_t		sc_ccb_mtx;
				/*
				 * this protects the ccb state and list entry
				 * between mpii_scsi_cmd and scsidone.
				 */

	struct workqueue	*sc_ssb_tmowk;

	struct mpii_dmamem	*sc_requests;

	struct mpii_dmamem	*sc_replies;
	struct mpii_rcb		*sc_rcbs;

	struct mpii_dmamem	*sc_reply_postq;
	struct mpii_reply_descr	*sc_reply_postq_kva;
	int			sc_reply_post_host_index;

	struct mpii_dmamem	*sc_reply_freeq;
	int			sc_reply_free_host_index;

	struct workqueue	*sc_ssb_evt_ackwk;

	struct sysmon_envsys	*sc_sme;
	envsys_data_t		*sc_sensors;
};

static int	mpii_match(device_t, cfdata_t, void *);
static void	mpii_attach(device_t, device_t, void *);
static int	mpii_detach(device_t, int);
static void	mpii_childdetached(device_t, device_t);
static int	mpii_rescan(device_t, const char *, const int *);

static int	mpii_intr(void *);

CFATTACH_DECL3_NEW(mpii, sizeof(struct mpii_softc),
    mpii_match, mpii_attach, mpii_detach, NULL, mpii_rescan,
    mpii_childdetached, DVF_DETACH_SHUTDOWN);

#define PREAD(s, r)	pci_conf_read((s)->sc_pc, (s)->sc_tag, (r))
#define PWRITE(s, r, v)	pci_conf_write((s)->sc_pc, (s)->sc_tag, (r), (v))

static void	mpii_scsipi_request(struct scsipi_channel *,
		    scsipi_adapter_req_t, void *);
static void	mpii_scsi_cmd_done(struct mpii_ccb *);
static void	mpii_minphys(struct buf *bp);

static struct mpii_dmamem *mpii_dmamem_alloc(struct mpii_softc *, size_t);
static void	mpii_dmamem_free(struct mpii_softc *, struct mpii_dmamem *);
static int	mpii_alloc_ccbs(struct mpii_softc *);
static struct mpii_ccb *mpii_get_ccb(struct mpii_softc *, int);
#define MPII_NOSLEEP 0x0001
static void	mpii_put_ccb(struct mpii_softc *, struct mpii_ccb *);
static int	mpii_alloc_replies(struct mpii_softc *);
static int	mpii_alloc_queues(struct mpii_softc *);
static void	mpii_push_reply(struct mpii_softc *, struct mpii_rcb *);
static void	mpii_push_replies(struct mpii_softc *);

static void	mpii_scsi_cmd_tmo(void *);
static void	mpii_scsi_cmd_tmo_handler(struct work *, void *);
static void	mpii_scsi_cmd_tmo_done(struct mpii_ccb *);

static int	mpii_alloc_dev(struct mpii_softc *);
static int	mpii_insert_dev(struct mpii_softc *, struct mpii_device *);
static int	mpii_remove_dev(struct mpii_softc *, struct mpii_device *);
static struct mpii_device *mpii_find_dev(struct mpii_softc *, u_int16_t);

static void	mpii_start(struct mpii_softc *, struct mpii_ccb *);
static int	mpii_poll(struct mpii_softc *, struct mpii_ccb *);
static void	mpii_poll_done(struct mpii_ccb *);
static struct mpii_rcb *mpii_reply(struct mpii_softc *,
			struct mpii_reply_descr *);

static void	mpii_wait(struct mpii_softc *, struct mpii_ccb *);
static void	mpii_wait_done(struct mpii_ccb *);

static void	mpii_init_queues(struct mpii_softc *);

static int	mpii_load_xs(struct mpii_ccb *);

static u_int32_t mpii_read(struct mpii_softc *, bus_size_t);
static void	mpii_write(struct mpii_softc *, bus_size_t, u_int32_t);
static int	mpii_wait_eq(struct mpii_softc *, bus_size_t, u_int32_t,
		    u_int32_t);
static int	mpii_wait_ne(struct mpii_softc *, bus_size_t, u_int32_t,
		    u_int32_t);

static int	mpii_init(struct mpii_softc *);
static int	mpii_reset_soft(struct mpii_softc *);
static int	mpii_reset_hard(struct mpii_softc *);

static int	mpii_handshake_send(struct mpii_softc *, void *, size_t);
static int	mpii_handshake_recv_dword(struct mpii_softc *,
		    u_int32_t *);
static int	mpii_handshake_recv(struct mpii_softc *, void *, size_t);

static void	mpii_empty_done(struct mpii_ccb *);

static int	mpii_iocinit(struct mpii_softc *);
static int	mpii_iocfacts(struct mpii_softc *);
static int	mpii_portfacts(struct mpii_softc *);
static int	mpii_portenable(struct mpii_softc *);
static int	mpii_cfg_coalescing(struct mpii_softc *);

static int	mpii_eventnotify(struct mpii_softc *);
static void	mpii_eventnotify_done(struct mpii_ccb *);
static void	mpii_eventack(struct work *, void *);
static void	mpii_eventack_done(struct mpii_ccb *);
static void	mpii_event_process(struct mpii_softc *, struct mpii_rcb *);
static void	mpii_event_sas(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
static void	mpii_event_raid(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
static void	mpii_event_defer(void *, void *);

static void	mpii_sas_remove_device(struct mpii_softc *, u_int16_t);

static int	mpii_req_cfg_header(struct mpii_softc *, u_int8_t,
		    u_int8_t, u_int32_t, int, void *);
static int	mpii_req_cfg_page(struct mpii_softc *, u_int32_t, int,
		    void *, int, void *, size_t);

static int	mpii_get_ioc_pg8(struct mpii_softc *);

#if 0
static int	mpii_ioctl_cache(struct scsi_link *, u_long, struct dk_cache *);
#endif
static int	mpii_cache_enable(struct mpii_softc *, struct mpii_device *);

#if NBIO > 0
static int	mpii_ioctl(device_t, u_long, void *);
static int	mpii_ioctl_inq(struct mpii_softc *, struct bioc_inq *);
static int	mpii_ioctl_vol(struct mpii_softc *, struct bioc_vol *);
static int	mpii_ioctl_disk(struct mpii_softc *, struct bioc_disk *);
static int	mpii_bio_hs(struct mpii_softc *, struct bioc_disk *, int,
		    int, int *);
static int	mpii_bio_disk(struct mpii_softc *, struct bioc_disk *,
		    u_int8_t);
static struct mpii_device *mpii_find_vol(struct mpii_softc *, int);
static int	mpii_bio_volstate(struct mpii_softc *, struct bioc_vol *);
static int	mpii_create_sensors(struct mpii_softc *);
static int	mpii_destroy_sensors(struct mpii_softc *);
static void	mpii_refresh_sensors(struct sysmon_envsys *, envsys_data_t *);
#endif /* NBIO > 0 */

#define DEVNAME(_s)		(device_xname((_s)->sc_dev))

#define dwordsof(s)		(sizeof(s) / sizeof(u_int32_t))
#define dwordn(p, n)		(((u_int32_t *)(p))[(n)])

#define mpii_read_db(s)		mpii_read((s), MPII_DOORBELL)
#define mpii_write_db(s, v)	mpii_write((s), MPII_DOORBELL, (v))
#define mpii_read_intr(s)	mpii_read((s), MPII_INTR_STATUS)
#define mpii_write_intr(s, v)	mpii_write((s), MPII_INTR_STATUS, (v))
#define mpii_reply_waiting(s)	((mpii_read_intr((s)) & MPII_INTR_STATUS_REPLY)\
				    == MPII_INTR_STATUS_REPLY)

#define mpii_read_reply_free(s)		mpii_read((s), \
						MPII_REPLY_FREE_HOST_INDEX)
#define mpii_write_reply_free(s, v)	mpii_write((s), \
						MPII_REPLY_FREE_HOST_INDEX, (v))
#define mpii_read_reply_post(s)		mpii_read((s), \
						MPII_REPLY_POST_HOST_INDEX)
#define mpii_write_reply_post(s, v)	mpii_write((s), \
						MPII_REPLY_POST_HOST_INDEX, (v))

#define mpii_wait_db_int(s)	mpii_wait_ne((s), MPII_INTR_STATUS, \
				    MPII_INTR_STATUS_IOC2SYSDB, 0)
#define mpii_wait_db_ack(s)	mpii_wait_eq((s), MPII_INTR_STATUS, \
				    MPII_INTR_STATUS_SYS2IOCDB, 0)

#define MPII_PG_EXTENDED	(1<<0)
#define MPII_PG_POLL		(1<<1)
#define MPII_PG_FMT		"\020" "\002POLL" "\001EXTENDED"

#define mpii_cfg_header(_s, _t, _n, _a, _h) \
	mpii_req_cfg_header((_s), (_t), (_n), (_a), \
	    MPII_PG_POLL, (_h))
#define mpii_ecfg_header(_s, _t, _n, _a, _h) \
	mpii_req_cfg_header((_s), (_t), (_n), (_a), \
	    MPII_PG_POLL|MPII_PG_EXTENDED, (_h))

#define mpii_cfg_page(_s, _a, _h, _r, _p, _l) \
	mpii_req_cfg_page((_s), (_a), MPII_PG_POLL, \
	    (_h), (_r), (_p), (_l))
#define mpii_ecfg_page(_s, _a, _h, _r, _p, _l) \
	mpii_req_cfg_page((_s), (_a), MPII_PG_POLL|MPII_PG_EXTENDED, \
	    (_h), (_r), (_p), (_l))


static const struct mpii_pci_product {
	pci_vendor_id_t         mpii_vendor;
	pci_product_id_t        mpii_product;
} mpii_devices[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2004 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2008 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_3 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_4 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_5 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2116_1 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2116_2 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_1 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_2 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_3 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_4 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_5 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2208_6 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2308_1 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2308_2 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2308_3 },
	{ 0,	0 }
};

static int
mpii_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct mpii_pci_product *mpii;

	for (mpii = mpii_devices; mpii->mpii_vendor != 0; mpii++) {
		if (PCI_VENDOR(pa->pa_id) == mpii->mpii_vendor &&
		    PCI_PRODUCT(pa->pa_id) == mpii->mpii_product)
			return (1);
	}
	return (0);
}

static void
mpii_attach(device_t parent, device_t self, void *aux)
{
	struct mpii_softc		*sc = device_private(self);
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	int				r;
	pci_intr_handle_t		ih;
	const char			*intrstr;
	struct mpii_ccb			*ccb;
	struct scsipi_adapter *adapt = &sc->sc_adapt;
	struct scsipi_channel *chan = &sc->sc_chan;
	char wkname[15];
	char intrbuf[PCI_INTRSTR_LEN];

	pci_aprint_devinfo(pa, NULL);

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_dev = self;
	
	mutex_init(&sc->sc_req_mtx, MUTEX_DEFAULT, IPL_BIO);
	mutex_init(&sc->sc_rep_mtx, MUTEX_DEFAULT, IPL_BIO);
	mutex_init(&sc->sc_ccb_free_mtx, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&sc->sc_ccb_free_cv, "mpii_ccbs");
	mutex_init(&sc->sc_ccb_mtx, MUTEX_DEFAULT, IPL_BIO);

	snprintf(wkname, sizeof(wkname), "%s_tmo", DEVNAME(sc));
	if (workqueue_create(&sc->sc_ssb_tmowk, wkname,
	    mpii_scsi_cmd_tmo_handler, sc, PRI_NONE, IPL_BIO, WQ_MPSAFE) != 0) {
		aprint_error_dev(self, "can't create %s workqueue\n", wkname);
		return;
	}

	snprintf(wkname, sizeof(wkname), "%s_evt", DEVNAME(sc));
	if (workqueue_create(&sc->sc_ssb_evt_ackwk, wkname,
	    mpii_eventack, sc, PRI_NONE, IPL_BIO, WQ_MPSAFE) != 0) {
		aprint_error_dev(self, "can't create %s workqueue\n", wkname);
		return;
	}

	/* find the appropriate memory base */
	for (r = PCI_MAPREG_START; r < PCI_MAPREG_END; r += sizeof(memtype)) {
		memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, r);
		if ((memtype & PCI_MAPREG_TYPE_MASK) == PCI_MAPREG_TYPE_MEM)
			break;
	}
	if (r >= PCI_MAPREG_END) {
		aprint_error_dev(self,
		    "unable to locate system interface registers\n");
		return;
	}

	if (pci_mapreg_map(pa, r, memtype, 0, &sc->sc_iot, &sc->sc_ioh,
	    NULL, &sc->sc_ios) != 0) {
		aprint_error_dev(self,
		    "unable to map system interface registers\n");
		return;
	}

	/* disable the expansion rom */
	PWRITE(sc, PCI_MAPREG_ROM,
	    PREAD(sc, PCI_MAPREG_ROM) & ~PCI_MAPREG_ROM_ENABLE);

	/* disable interrupts */
	mpii_write(sc, MPII_INTR_MASK,
	    MPII_INTR_MASK_RESET | MPII_INTR_MASK_REPLY |
	    MPII_INTR_MASK_DOORBELL);

	/* hook up the interrupt */
	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(self, "unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));

	if (mpii_init(sc) != 0) {
		aprint_error_dev(self, "unable to initialize ioc\n");
		goto unmap;
	}

	if (mpii_iocfacts(sc) != 0) {
		aprint_error_dev(self, "unable to get iocfacts\n");
		goto unmap;
	}

	if (mpii_alloc_ccbs(sc) != 0) {
		/* error already printed */
		goto unmap;
	}

	if (mpii_alloc_replies(sc) != 0) {
		aprint_error_dev(self, "unable to allocated reply space\n");
		goto free_ccbs;
	}

	if (mpii_alloc_queues(sc) != 0) {
		aprint_error_dev(self, "unable to allocate reply queues\n");
		goto free_replies;
	}

	if (mpii_iocinit(sc) != 0) {
		aprint_error_dev(self, "unable to send iocinit\n");
		goto free_queues;
	}

	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_OPER) != 0) {
		aprint_error_dev(self, "state: 0x%08x\n",
			mpii_read_db(sc) & MPII_DOORBELL_STATE);
		aprint_error_dev(self, "operational state timeout\n");
		goto free_queues;
	}

	mpii_push_replies(sc);
	mpii_init_queues(sc);

	if (mpii_portfacts(sc) != 0) {
		aprint_error_dev(self, "unable to get portfacts\n");
		goto free_queues;
	}

	if (mpii_get_ioc_pg8(sc) != 0) {
		aprint_error_dev(self, "unable to get ioc page 8\n");
		goto free_queues;
	}

	if (mpii_cfg_coalescing(sc) != 0) {
		aprint_error_dev(self, "unable to configure coalescing\n");
		goto free_queues;
	}

	/* XXX bail on unsupported porttype? */
	if ((sc->sc_porttype == MPII_PORTFACTS_PORTTYPE_SAS_PHYSICAL) ||
	    (sc->sc_porttype == MPII_PORTFACTS_PORTTYPE_SAS_VIRTUAL)) {
		if (mpii_eventnotify(sc) != 0) {
			aprint_error_dev(self, "unable to enable events\n");
			goto free_queues;
		}
	}

	if (mpii_alloc_dev(sc) != 0) {
		aprint_error_dev(self,
		    "unable to allocate memory for mpii_device\n");
		goto free_queues;
	}

	if (mpii_portenable(sc) != 0) {
		aprint_error_dev(self, "unable to enable port\n");
		goto free_dev;
	}

	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
	    mpii_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrstr)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto free_dev;
	}

	memset(adapt, 0, sizeof(*adapt));
	adapt->adapt_dev = sc->sc_dev;
	adapt->adapt_nchannels = 1;
	adapt->adapt_openings = sc->sc_request_depth - 1;
	adapt->adapt_max_periph = adapt->adapt_openings;
	adapt->adapt_request = mpii_scsipi_request;
	adapt->adapt_minphys = mpii_minphys;

	memset(chan, 0, sizeof(*chan));
	chan->chan_adapter = adapt;
	chan->chan_bustype = &scsi_sas_bustype;
	chan->chan_channel = 0;
	chan->chan_flags = 0;
	chan->chan_nluns = 8;
	chan->chan_ntargets = sc->sc_max_devices;
	chan->chan_id = -1;

	mpii_rescan(self, "scsi", NULL);

	/* enable interrupts */
	mpii_write(sc, MPII_INTR_MASK, MPII_INTR_MASK_DOORBELL
	    | MPII_INTR_MASK_RESET);

#if NBIO > 0
	if (ISSET(sc->sc_flags, MPII_F_RAID)) {
		if (bio_register(sc->sc_dev, mpii_ioctl) != 0)
			panic("%s: controller registration failed",
			    DEVNAME(sc));

		if (mpii_create_sensors(sc) != 0)
			aprint_error_dev(self, "unable to create sensors\n");
	}
#endif

	return;

free_dev:
	if (sc->sc_devs)
		free(sc->sc_devs, M_DEVBUF);

free_queues:
	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_freeq),
     	    0, sc->sc_reply_free_qdepth * 4, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_reply_freeq);

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq),
	    0, sc->sc_reply_post_qdepth * 8, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_reply_postq);

free_replies:
	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_replies),
		0, PAGE_SIZE, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_replies);

free_ccbs:
	while ((ccb = mpii_get_ccb(sc, MPII_NOSLEEP)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	mpii_dmamem_free(sc, sc->sc_requests);
	free(sc->sc_ccbs, M_DEVBUF);

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

static int
mpii_detach(device_t self, int flags)
{
	struct mpii_softc		*sc = device_private(self);
	int error;
	struct mpii_ccb *ccb;

	if ((error = config_detach_children(sc->sc_dev, flags)) != 0)
		return error;

#if NBIO > 0
	mpii_destroy_sensors(sc);
	bio_unregister(sc->sc_dev);
#endif /* NBIO > 0 */

	if (sc->sc_ih != NULL) {
		if (sc->sc_devs)
			free(sc->sc_devs, M_DEVBUF);

		bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_freeq),
		    0, sc->sc_reply_free_qdepth * 4, BUS_DMASYNC_POSTREAD);
		mpii_dmamem_free(sc, sc->sc_reply_freeq);

		bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq),
		    0, sc->sc_reply_post_qdepth * 8, BUS_DMASYNC_POSTREAD);
		mpii_dmamem_free(sc, sc->sc_reply_postq);

		bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_replies),
			0, PAGE_SIZE, BUS_DMASYNC_POSTREAD);
		mpii_dmamem_free(sc, sc->sc_replies);

		while ((ccb = mpii_get_ccb(sc, MPII_NOSLEEP)) != NULL)
			bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
		mpii_dmamem_free(sc, sc->sc_requests);
		free(sc->sc_ccbs, M_DEVBUF);

		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc_ios != 0) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
		sc->sc_ios = 0;
	}

	return (0);
}

static int
mpii_rescan(device_t self, const char *ifattr, const int *locators)
{
	struct mpii_softc *sc = device_private(self);

	if (sc->sc_child != NULL)
		return 0;

	sc->sc_child = config_found_sm_loc(self, ifattr, locators, &sc->sc_chan,
	    scsiprint, NULL);

	return 0;
}

static void
mpii_childdetached(device_t self, device_t child)
{
        struct mpii_softc *sc = device_private(self);

        KASSERT(self == sc->sc_dev);
        KASSERT(child == sc->sc_child);

        if (child == sc->sc_child)
                sc->sc_child = NULL;
}

static int
mpii_intr(void *arg)
{
	struct mpii_rcb_list		evts = SIMPLEQ_HEAD_INITIALIZER(evts);
	struct mpii_ccb_list		ccbs = SIMPLEQ_HEAD_INITIALIZER(ccbs);
	struct mpii_softc		*sc = arg;
	struct mpii_reply_descr		*postq = sc->sc_reply_postq_kva, *rdp;
	struct mpii_ccb			*ccb;
	struct mpii_rcb			*rcb;
	int				smid;
	int				rv = 0;

	mutex_enter(&sc->sc_rep_mtx);
	bus_dmamap_sync(sc->sc_dmat,
	    MPII_DMA_MAP(sc->sc_reply_postq),
	    0, 8 * sc->sc_reply_post_qdepth,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (;;) {
		rdp = &postq[sc->sc_reply_post_host_index];
		if ((rdp->reply_flags & MPII_REPLY_DESCR_TYPE_MASK) ==
		    MPII_REPLY_DESCR_UNUSED)
			break;
		if (rdp->data == 0xffffffff) {
			/*
			 * ioc is still writing to the reply post queue
			 * race condition - bail!
			 */
			break;
		}

		smid = le16toh(rdp->smid);
		rcb = mpii_reply(sc, rdp);

		if (smid) {
			ccb = &sc->sc_ccbs[smid - 1];
			ccb->ccb_state = MPII_CCB_READY;
			ccb->ccb_rcb = rcb;
			SIMPLEQ_INSERT_TAIL(&ccbs, ccb, u.ccb_link);
		} else
			SIMPLEQ_INSERT_TAIL(&evts, rcb, u.rcb_link);

		sc->sc_reply_post_host_index++;
		sc->sc_reply_post_host_index %= sc->sc_reply_post_qdepth;
		rv = 1;
	}

	bus_dmamap_sync(sc->sc_dmat,
	    MPII_DMA_MAP(sc->sc_reply_postq),
	    0, 8 * sc->sc_reply_post_qdepth,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (rv)
		mpii_write_reply_post(sc, sc->sc_reply_post_host_index);

	mutex_exit(&sc->sc_rep_mtx);

	if (rv == 0)
		return (0);

	while ((ccb = SIMPLEQ_FIRST(&ccbs)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&ccbs, u.ccb_link);
		ccb->ccb_done(ccb);
	}
	while ((rcb = SIMPLEQ_FIRST(&evts)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&evts, u.rcb_link);
		mpii_event_process(sc, rcb);
	}

	return (1);
}

static int
mpii_load_xs(struct mpii_ccb *ccb)
{
	struct mpii_softc	*sc = ccb->ccb_sc;
	struct scsipi_xfer	*xs = ccb->ccb_cookie;
	struct mpii_ccb_bundle	*mcb = ccb->ccb_cmd;
	struct mpii_msg_scsi_io	*io = &mcb->mcb_io;
	struct mpii_sge		*sge = NULL, *nsge = &mcb->mcb_sgl[0];
	struct mpii_sge		*ce = NULL, *nce = NULL;
	u_int64_t		ce_dva;
	bus_dmamap_t		dmap = ccb->ccb_dmamap;
	u_int32_t		addr, flags;
	int			i, error;

	/* zero length transfer still requires an SGE */
	if (xs->datalen == 0) {
		nsge->sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
		    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL);
		return (0);
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xs->data, xs->datalen, NULL, (xs->xs_control & XS_CTL_NOSLEEP) ?
	      BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		aprint_error_dev(sc->sc_dev, "error %d loading dmamap\n",
		    error);
		return (1);
	}

	/* safe default staring flags */
	flags = MPII_SGE_FL_TYPE_SIMPLE | MPII_SGE_FL_SIZE_64;
	/* if data out */
	if (xs->xs_control & XS_CTL_DATA_OUT)
		flags |= MPII_SGE_FL_DIR_OUT;

	/* we will have to exceed the SGEs we can cram into the request frame */
	if (dmap->dm_nsegs > sc->sc_first_sgl_len) {
		ce = &mcb->mcb_sgl[sc->sc_first_sgl_len - 1];
		io->chain_offset = ((u_int8_t *)ce - (u_int8_t *)io) / 4;
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		if (nsge == ce) {
			nsge++;
			sge->sg_hdr |= htole32(MPII_SGE_FL_LAST);

			DNPRINTF(MPII_D_DMA, "%s:   - 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), sge->sg_hdr,
			    sge->sg_hi_addr, sge->sg_lo_addr);

			if ((dmap->dm_nsegs - i) > sc->sc_chain_len) {
				nce = &nsge[sc->sc_chain_len - 1];
				addr = ((u_int8_t *)nce - (u_int8_t *)nsge) / 4;
				addr = addr << 16 |
				    sizeof(struct mpii_sge) * sc->sc_chain_len;
			} else {
				nce = NULL;
				addr = sizeof(struct mpii_sge) *
				    (dmap->dm_nsegs - i);
			}

			ce->sg_hdr = htole32(MPII_SGE_FL_TYPE_CHAIN |
			    MPII_SGE_FL_SIZE_64 | addr);

			ce_dva = ccb->ccb_cmd_dva +
			    ((u_int8_t *)nsge - (u_int8_t *)mcb);

			addr = (u_int32_t)(ce_dva >> 32);
			ce->sg_hi_addr = htole32(addr);
			addr = (u_int32_t)ce_dva;
			ce->sg_lo_addr = htole32(addr);

			DNPRINTF(MPII_D_DMA, "%s:  ce: 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), ce->sg_hdr, ce->sg_hi_addr,
			    ce->sg_lo_addr);

			ce = nce;
		}

		DNPRINTF(MPII_D_DMA, "%s:  %d: %" PRId64 " 0x%016" PRIx64 "\n",
		    DEVNAME(sc), i, (int64_t)dmap->dm_segs[i].ds_len,
		    (u_int64_t)dmap->dm_segs[i].ds_addr);

		sge = nsge;

		sge->sg_hdr = htole32(flags | dmap->dm_segs[i].ds_len);
		addr = (u_int32_t)((u_int64_t)dmap->dm_segs[i].ds_addr >> 32);
		sge->sg_hi_addr = htole32(addr);
		addr = (u_int32_t)dmap->dm_segs[i].ds_addr;
		sge->sg_lo_addr = htole32(addr);

		DNPRINTF(MPII_D_DMA, "%s:  %d: 0x%08x 0x%08x 0x%08x\n",
		    DEVNAME(sc), i, sge->sg_hdr, sge->sg_hi_addr,
		    sge->sg_lo_addr);

		nsge = sge + 1;
	}

	/* terminate list */
	sge->sg_hdr |= htole32(MPII_SGE_FL_LAST | MPII_SGE_FL_EOB |
	    MPII_SGE_FL_EOL);

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

static u_int32_t
mpii_read(struct mpii_softc *sc, bus_size_t r)
{
	u_int32_t			rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MPII_D_RW, "%s: mpii_read %#" PRIx64 " %#x\n", DEVNAME(sc),
	    (uint64_t)r, rv);

	return (rv);
}

static void
mpii_write(struct mpii_softc *sc, bus_size_t r, u_int32_t v)
{
	DNPRINTF(MPII_D_RW, "%s: mpii_write %#" PRIx64 " %#x\n", DEVNAME(sc),
	    (uint64_t)r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}


static int
mpii_wait_eq(struct mpii_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int			i;

	DNPRINTF(MPII_D_RW, "%s: mpii_wait_eq %#" PRIx64 " %#x %#x\n",
	    DEVNAME(sc), (uint64_t)r, mask, target);

	for (i = 0; i < 15000; i++) {
		if ((mpii_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

static int
mpii_wait_ne(struct mpii_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int			i;

	DNPRINTF(MPII_D_RW, "%s: mpii_wait_ne %#" PRIx64 " %#x %#x\n",
	    DEVNAME(sc), (uint64_t)r, mask, target);

	for (i = 0; i < 15000; i++) {
		if ((mpii_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}


static int
mpii_init(struct mpii_softc *sc)
{
	u_int32_t		db;
	int			i;

	/* spin until the ioc leaves the reset state */
	if (mpii_wait_ne(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_RESET) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_init timeout waiting to leave "
		    "reset state\n", DEVNAME(sc));
		return (1);
	}

	/* check current ownership */
	db = mpii_read_db(sc);
	if ((db & MPII_DOORBELL_WHOINIT) == MPII_DOORBELL_WHOINIT_PCIPEER) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_init initialised by pci peer\n",
		    DEVNAME(sc));
		return (0);
	}

	for (i = 0; i < 5; i++) {
		switch (db & MPII_DOORBELL_STATE) {
		case MPII_DOORBELL_STATE_READY:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is ready\n",
			    DEVNAME(sc));
			return (0);

		case MPII_DOORBELL_STATE_OPER:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is oper\n",
			    DEVNAME(sc));
			if (sc->sc_ioc_event_replay)
				mpii_reset_soft(sc);
			else
				mpii_reset_hard(sc);
			break;

		case MPII_DOORBELL_STATE_FAULT:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is being "
			    "reset hard\n" , DEVNAME(sc));
			mpii_reset_hard(sc);
			break;

		case MPII_DOORBELL_STATE_RESET:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init waiting to come "
			    "out of reset\n", DEVNAME(sc));
			if (mpii_wait_ne(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
			    MPII_DOORBELL_STATE_RESET) != 0)
				return (1);
			break;
		}
		db = mpii_read_db(sc);
	}

	return (1);
}

static int
mpii_reset_soft(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s: mpii_reset_soft\n", DEVNAME(sc));

	if (mpii_read_db(sc) & MPII_DOORBELL_INUSE) {
		return (1);
	}

	mpii_write_db(sc,
	    MPII_DOORBELL_FUNCTION(MPII_FUNCTION_IOC_MESSAGE_UNIT_RESET));
	
	/* XXX LSI waits 15 sec */
	if (mpii_wait_db_ack(sc) != 0)
		return (1);

	/* XXX LSI waits 15 sec */
	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_READY) != 0)
		return (1);

	/* XXX wait for Sys2IOCDB bit to clear in HIS?? */

	return (0);
}

static int
mpii_reset_hard(struct mpii_softc *sc)
{
	u_int16_t		i;

	DNPRINTF(MPII_D_MISC, "%s: mpii_reset_hard\n", DEVNAME(sc));

	mpii_write_intr(sc, 0);

	/* enable diagnostic register */
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_FLUSH);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_1);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_2);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_3);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_4);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_5);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_6);

	delay(100);

	if ((mpii_read(sc, MPII_HOSTDIAG) & MPII_HOSTDIAG_DWRE) == 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_reset_hard failure to enable "
		    "diagnostic read/write\n", DEVNAME(sc));
		return(1);
	}

	/* reset ioc */
	mpii_write(sc, MPII_HOSTDIAG, MPII_HOSTDIAG_RESET_ADAPTER);

	/* 240 milliseconds */
	delay(240000);


	/* XXX this whole function should be more robust */
	
	/* XXX  read the host diagnostic reg until reset adapter bit clears ? */
	for (i = 0; i < 30000; i++) {
		if ((mpii_read(sc, MPII_HOSTDIAG) &
		    MPII_HOSTDIAG_RESET_ADAPTER) == 0)
			break;
		delay(10000);
	}

	/* disable diagnostic register */
	mpii_write(sc, MPII_WRITESEQ, 0xff);

	/* XXX what else? */

	DNPRINTF(MPII_D_MISC, "%s: done with mpii_reset_hard\n", DEVNAME(sc));

	return(0);
}

static int
mpii_handshake_send(struct mpii_softc *sc, void *buf, size_t dwords)
{
	u_int32_t		*query = buf;
	int			i;

	/* make sure the doorbell is not in use. */
	if (mpii_read_db(sc) & MPII_DOORBELL_INUSE)
		return (1);

	/* clear pending doorbell interrupts */
	if (mpii_read_intr(sc) & MPII_INTR_STATUS_IOC2SYSDB)
		mpii_write_intr(sc, 0);

	/*
	 * first write the doorbell with the handshake function and the
	 * dword count.
	 */
	mpii_write_db(sc, MPII_DOORBELL_FUNCTION(MPII_FUNCTION_HANDSHAKE) |
	    MPII_DOORBELL_DWORDS(dwords));

	/*
	 * the doorbell used bit will be set because a doorbell function has
	 * started. wait for the interrupt and then ack it.
	 */
	if (mpii_wait_db_int(sc) != 0)
		return (1);
	mpii_write_intr(sc, 0);

	/* poll for the acknowledgement. */
	if (mpii_wait_db_ack(sc) != 0)
		return (1);

	/* write the query through the doorbell. */
	for (i = 0; i < dwords; i++) {
		mpii_write_db(sc, htole32(query[i]));
		if (mpii_wait_db_ack(sc) != 0)
			return (1);
	}

	return (0);
}

static int
mpii_handshake_recv_dword(struct mpii_softc *sc, u_int32_t *dword)
{
	u_int16_t		*words = (u_int16_t *)dword;
	int			i;

	for (i = 0; i < 2; i++) {
		if (mpii_wait_db_int(sc) != 0)
			return (1);
		words[i] = le16toh(mpii_read_db(sc) & MPII_DOORBELL_DATA_MASK);
		mpii_write_intr(sc, 0);
	}

	return (0);
}

static int
mpii_handshake_recv(struct mpii_softc *sc, void *buf, size_t dwords)
{
	struct mpii_msg_reply	*reply = buf;
	u_int32_t		*dbuf = buf, dummy;
	int			i;

	/* get the first dword so we can read the length out of the header. */
	if (mpii_handshake_recv_dword(sc, &dbuf[0]) != 0)
		return (1);

	DNPRINTF(MPII_D_CMD, "%s: mpii_handshake_recv dwords: %zd reply: %d\n",
	    DEVNAME(sc), dwords, reply->msg_length);

	/*
	 * the total length, in dwords, is in the message length field of the
	 * reply header.
	 */
	for (i = 1; i < MIN(dwords, reply->msg_length); i++) {
		if (mpii_handshake_recv_dword(sc, &dbuf[i]) != 0)
			return (1);
	}

	/* if there's extra stuff to come off the ioc, discard it */
	while (i++ < reply->msg_length) {
		if (mpii_handshake_recv_dword(sc, &dummy) != 0)
			return (1);
		DNPRINTF(MPII_D_CMD, "%s: mpii_handshake_recv dummy read: "
		    "0x%08x\n", DEVNAME(sc), dummy);
	}

	/* wait for the doorbell used bit to be reset and clear the intr */
	if (mpii_wait_db_int(sc) != 0)
		return (1);
	
	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_INUSE, 0) != 0)
		return (1);
	
	mpii_write_intr(sc, 0);

	return (0);
}

static void
mpii_empty_done(struct mpii_ccb *ccb)
{
	/* nothing to do */
}

static int
mpii_iocfacts(struct mpii_softc *sc)
{
	struct mpii_msg_iocfacts_request	ifq;
	struct mpii_msg_iocfacts_reply		ifp;

	DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts\n", DEVNAME(sc));

	bzero(&ifq, sizeof(ifq));
	bzero(&ifp, sizeof(ifp));

	ifq.function = MPII_FUNCTION_IOC_FACTS;

	if (mpii_handshake_send(sc, &ifq, dwordsof(ifq)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpii_handshake_recv(sc, &ifp, dwordsof(ifp)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s:  func: 0x%02x length: %d msgver: %d.%d\n",
	    DEVNAME(sc), ifp.function, ifp.msg_length,
	    ifp.msg_version_maj, ifp.msg_version_min);
	DNPRINTF(MPII_D_MISC, "%s:  msgflags: 0x%02x iocnumber: 0x%02x "
	    "headerver: %d.%d\n", DEVNAME(sc), ifp.msg_flags,
	    ifp.ioc_number, ifp.header_version_unit,
	    ifp.header_version_dev);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    ifp.vp_id, ifp.vf_id);
	DNPRINTF(MPII_D_MISC, "%s:  iocstatus: 0x%04x ioexceptions: 0x%04x\n",
	    DEVNAME(sc), le16toh(ifp.ioc_status),
	    le16toh(ifp.ioc_exceptions));
	DNPRINTF(MPII_D_MISC, "%s:  iocloginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(ifp.ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  numberofports: 0x%02x whoinit: 0x%02x "
	    "maxchaindepth: %d\n", DEVNAME(sc), ifp.number_of_ports,
	    ifp.whoinit, ifp.max_chain_depth);
	DNPRINTF(MPII_D_MISC, "%s:  productid: 0x%04x requestcredit: 0x%04x\n",
	    DEVNAME(sc), le16toh(ifp.product_id), le16toh(ifp.request_credit));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_capabilities: 0x%08x\n", DEVNAME(sc),
	    le32toh(ifp.ioc_capabilities));
	DNPRINTF(MPII_D_MISC, "%s:  fw_version: %d.%d fw_version_unit: 0x%02x "
	    "fw_version_dev: 0x%02x\n", DEVNAME(sc),
	    ifp.fw_version_maj, ifp.fw_version_min,
	    ifp.fw_version_unit, ifp.fw_version_dev);
	DNPRINTF(MPII_D_MISC, "%s:  iocrequestframesize: 0x%04x\n",
	    DEVNAME(sc), le16toh(ifp.ioc_request_frame_size));
	DNPRINTF(MPII_D_MISC, "%s:  maxtargets: 0x%04x "
	    "maxinitiators: 0x%04x\n", DEVNAME(sc),
	    le16toh(ifp.max_targets), le16toh(ifp.max_initiators));
	DNPRINTF(MPII_D_MISC, "%s:  maxenclosures: 0x%04x "
	    "maxsasexpanders: 0x%04x\n", DEVNAME(sc),
	    le16toh(ifp.max_enclosures), le16toh(ifp.max_sas_expanders));
	DNPRINTF(MPII_D_MISC, "%s:  highprioritycredit: 0x%04x "
	    "protocolflags: 0x%02x\n", DEVNAME(sc),
	    le16toh(ifp.high_priority_credit), le16toh(ifp.protocol_flags));
	DNPRINTF(MPII_D_MISC, "%s:  maxvolumes: 0x%02x replyframesize: 0x%02x "
	    "mrdpqd: 0x%04x\n", DEVNAME(sc), ifp.max_volumes,
	    ifp.reply_frame_size,
	    le16toh(ifp.max_reply_descriptor_post_queue_depth));
	DNPRINTF(MPII_D_MISC, "%s:  maxpersistententries: 0x%04x "
	    "maxdevhandle: 0x%02x\n", DEVNAME(sc),
	    le16toh(ifp.max_persistent_entries), le16toh(ifp.max_dev_handle));

	sc->sc_maxchdepth = ifp.max_chain_depth;
	sc->sc_ioc_number = ifp.ioc_number;
	sc->sc_vf_id = ifp.vf_id;

	sc->sc_num_ports = ifp.number_of_ports;
	sc->sc_ioc_event_replay = (le32toh(ifp.ioc_capabilities) &
	    MPII_IOCFACTS_CAPABILITY_EVENT_REPLAY) ? 1 : 0;
	sc->sc_max_enclosures = le16toh(ifp.max_enclosures);
	sc->sc_max_expanders = le16toh(ifp.max_sas_expanders);
	sc->sc_max_volumes = ifp.max_volumes;
	sc->sc_max_devices = ifp.max_volumes + le16toh(ifp.max_targets);
	sc->sc_num_channels = 1;

	if (ISSET(le32toh(ifp.ioc_capabilities),
	    MPII_IOCFACTS_CAPABILITY_INTEGRATED_RAID))
		SET(sc->sc_flags, MPII_F_RAID);

	sc->sc_request_depth = MIN(le16toh(ifp.request_credit),
	    MPII_MAX_REQUEST_CREDIT);

	/* should not be multiple of 16 */
	sc->sc_num_reply_frames = sc->sc_request_depth + 32;
	if (!(sc->sc_num_reply_frames % 16))
		sc->sc_num_reply_frames--;

	/* must be multiple of 16 */
	sc->sc_reply_free_qdepth = sc->sc_num_reply_frames +
	    (16 - (sc->sc_num_reply_frames % 16));
	sc->sc_reply_post_qdepth = ((sc->sc_request_depth +
	    sc->sc_num_reply_frames + 1 + 15) / 16) * 16;

	if (sc->sc_reply_post_qdepth >
	    ifp.max_reply_descriptor_post_queue_depth)
		sc->sc_reply_post_qdepth =
		    ifp.max_reply_descriptor_post_queue_depth;

	DNPRINTF(MPII_D_MISC, "%s: sc_request_depth: %d "
	    "sc_num_reply_frames: %d sc_reply_free_qdepth: %d "
	    "sc_reply_post_qdepth: %d\n", DEVNAME(sc), sc->sc_request_depth,
	    sc->sc_num_reply_frames, sc->sc_reply_free_qdepth,
	    sc->sc_reply_post_qdepth);

	/*
	 * you can fit sg elements on the end of the io cmd if they fit in the
	 * request frame size.
	 */

	sc->sc_first_sgl_len = ((le16toh(ifp.ioc_request_frame_size) * 4) -
	    sizeof(struct mpii_msg_scsi_io)) / sizeof(struct mpii_sge);
	DNPRINTF(MPII_D_MISC, "%s:   first sgl len: %d\n", DEVNAME(sc),
	    sc->sc_first_sgl_len);

	sc->sc_chain_len = (le16toh(ifp.ioc_request_frame_size) * 4) /
	    sizeof(struct mpii_sge);
	DNPRINTF(MPII_D_MISC, "%s:   chain len: %d\n", DEVNAME(sc),
	    sc->sc_chain_len);

	/* the sgl tailing the io cmd loses an entry to the chain element. */
	sc->sc_max_sgl_len = MPII_MAX_SGL - 1;
	/* the sgl chains lose an entry for each chain element */
	sc->sc_max_sgl_len -= (MPII_MAX_SGL - sc->sc_first_sgl_len) /
	    sc->sc_chain_len;
	DNPRINTF(MPII_D_MISC, "%s:   max sgl len: %d\n", DEVNAME(sc),
	    sc->sc_max_sgl_len);

	/* XXX we're ignoring the max chain depth */

	return(0);

}

static int
mpii_iocinit(struct mpii_softc *sc)
{
	struct mpii_msg_iocinit_request		iiq;
	struct mpii_msg_iocinit_reply		iip;
	u_int32_t				hi_addr;

	DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit\n", DEVNAME(sc));

	bzero(&iiq, sizeof(iiq));
	bzero(&iip, sizeof(iip));

	iiq.function = MPII_FUNCTION_IOC_INIT;
	iiq.whoinit = MPII_WHOINIT_HOST_DRIVER;
	
	/* XXX JPG do something about vf_id */
	iiq.vf_id = 0;

	iiq.msg_version_maj = 0x02;
	iiq.msg_version_min = 0x00;

	/* XXX JPG ensure compliance with some level and hard-code? */
	iiq.hdr_version_unit = 0x00;
	iiq.hdr_version_dev = 0x00;

	iiq.system_request_frame_size = htole16(MPII_REQUEST_SIZE / 4);

	iiq.reply_descriptor_post_queue_depth =
	    htole16(sc->sc_reply_post_qdepth);

	iiq.reply_free_queue_depth = htole16(sc->sc_reply_free_qdepth);
	
	hi_addr = (u_int32_t)((u_int64_t)MPII_DMA_DVA(sc->sc_requests) >> 32);
	iiq.sense_buffer_address_high = htole32(hi_addr);

	hi_addr = (u_int32_t)
	    ((u_int64_t)MPII_DMA_DVA(sc->sc_replies) >> 32);
	iiq.system_reply_address_high = htole32(hi_addr);

	iiq.system_request_frame_base_address =
	    (u_int64_t)MPII_DMA_DVA(sc->sc_requests);

	iiq.reply_descriptor_post_queue_address =
	    (u_int64_t)MPII_DMA_DVA(sc->sc_reply_postq);

	iiq.reply_free_queue_address =
	    (u_int64_t)MPII_DMA_DVA(sc->sc_reply_freeq);

	if (mpii_handshake_send(sc, &iiq, dwordsof(iiq)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpii_handshake_recv(sc, &iip, dwordsof(iip)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s:  function: 0x%02x msg_length: %d "
	    "whoinit: 0x%02x\n", DEVNAME(sc), iip.function,
	    iip.msg_length, iip.whoinit);
	DNPRINTF(MPII_D_MISC, "%s:  msg_flags: 0x%02x\n", DEVNAME(sc),
	    iip.msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vf_id: 0x%02x vp_id: 0x%02x\n", DEVNAME(sc),
	    iip.vf_id, iip.vp_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    le16toh(iip.ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(iip.ioc_loginfo));

	if ((iip.ioc_status != MPII_IOCSTATUS_SUCCESS) || (iip.ioc_loginfo))
		return (1);

	return (0);
}

static void
mpii_push_reply(struct mpii_softc *sc, struct mpii_rcb *rcb)
{
	u_int32_t		*rfp;

	if (rcb == NULL)
		return;

	rfp = MPII_DMA_KVA(sc->sc_reply_freeq);
	rfp[sc->sc_reply_free_host_index] = rcb->rcb_reply_dva;

	sc->sc_reply_free_host_index = (sc->sc_reply_free_host_index + 1) %
	    sc->sc_reply_free_qdepth;

	mpii_write_reply_free(sc, sc->sc_reply_free_host_index);
}

static int
mpii_portfacts(struct mpii_softc *sc)
{
	struct mpii_msg_portfacts_request	*pfq;
	struct mpii_msg_portfacts_reply		*pfp;
	struct mpii_ccb				*ccb;
	int					rv = 1;

	DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts\n", DEVNAME(sc));

	ccb = mpii_get_ccb(sc, 0);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts mpii_get_ccb fail\n",
		    DEVNAME(sc));
		return (rv);
	}

	ccb->ccb_done = mpii_empty_done;
	pfq = ccb->ccb_cmd;

	bzero(pfq, sizeof(*pfq));

	pfq->function = MPII_FUNCTION_PORT_FACTS;
	pfq->chain_offset = 0;
	pfq->msg_flags = 0;
	pfq->port_number = 0;
	pfq->vp_id = 0;
	pfq->vf_id = 0;

	if (mpii_poll(sc, ccb) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts poll\n",
		    DEVNAME(sc));
		goto err;
	}

	if (ccb->ccb_rcb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: empty portfacts reply\n",
		    DEVNAME(sc));
		goto err;
	}

	pfp = ccb->ccb_rcb->rcb_reply;
	DNPRINTF(MPII_D_MISC, "%s   pfp: %p\n", DEVNAME(sc), pfp);

	DNPRINTF(MPII_D_MISC, "%s:  function: 0x%02x msg_length: %d\n",
	    DEVNAME(sc), pfp->function, pfp->msg_length);
	DNPRINTF(MPII_D_MISC, "%s:  msg_flags: 0x%02x port_number: %d\n",
	    DEVNAME(sc), pfp->msg_flags, pfp->port_number);
	DNPRINTF(MPII_D_MISC, "%s:  vf_id: 0x%02x vp_id: 0x%02x\n",
	    DEVNAME(sc), pfp->vf_id, pfp->vp_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    le16toh(pfp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(pfp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  port_type: 0x%02x\n", DEVNAME(sc),
	    pfp->port_type);
	DNPRINTF(MPII_D_MISC, "%s:  max_posted_cmd_buffers: %d\n", DEVNAME(sc),
	    le16toh(pfp->max_posted_cmd_buffers));

	sc->sc_porttype = pfp->port_type;

	mpii_push_reply(sc, ccb->ccb_rcb);
	rv = 0;
err:
	mpii_put_ccb(sc, ccb);

	return (rv);
}

static void
mpii_eventack(struct work *wk, void *cookie)
{
	struct mpii_softc			*sc = cookie;
	struct mpii_ccb				*ccb;
	struct mpii_rcb				*rcb = (void *)wk;
	struct mpii_msg_event_reply		*enp;
	struct mpii_msg_eventack_request	*eaq;

	ccb = mpii_get_ccb(sc, 0);

	enp = (struct mpii_msg_event_reply *)rcb->rcb_reply;

	ccb->ccb_done = mpii_eventack_done;
	eaq = ccb->ccb_cmd;

	eaq->function = MPII_FUNCTION_EVENT_ACK;

	eaq->event = enp->event;
	eaq->event_context = enp->event_context;

	mpii_push_reply(sc, rcb);

	mpii_start(sc, ccb);

}

static void
mpii_eventack_done(struct mpii_ccb *ccb)
{
	struct mpii_softc			*sc = ccb->ccb_sc;

	DNPRINTF(MPII_D_EVT, "%s: event ack done\n", DEVNAME(sc));

	mpii_push_reply(sc, ccb->ccb_rcb);
	mpii_put_ccb(sc, ccb);
}

static int
mpii_portenable(struct mpii_softc *sc)
{
	struct mpii_msg_portenable_request	*peq;
	struct mpii_ccb				*ccb;

	DNPRINTF(MPII_D_MISC, "%s: mpii_portenable\n", DEVNAME(sc));

	ccb = mpii_get_ccb(sc, 0);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portenable ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpii_empty_done;
	peq = ccb->ccb_cmd;

	peq->function = MPII_FUNCTION_PORT_ENABLE;
	peq->vf_id = sc->sc_vf_id;

	if (mpii_poll(sc, ccb) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portenable poll\n",
		    DEVNAME(sc));
		return (1);
	}

	if (ccb->ccb_rcb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: empty portenable reply\n",
		    DEVNAME(sc));
		return (1);
	}

	mpii_push_reply(sc, ccb->ccb_rcb);
	mpii_put_ccb(sc, ccb);

	return (0);
}

static int
mpii_cfg_coalescing(struct mpii_softc *sc)
{
	struct mpii_cfg_hdr		hdr;
	struct mpii_cfg_ioc_pg1		pg;

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_IOC, 1, 0,
	    &hdr) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch IOC page 1 "
		    "header\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_cfg_page(sc, 0, &hdr, 1, &pg, sizeof(pg)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch IOC page 1\n"
		    "page 1\n", DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s: IOC page 1\n", DEVNAME(sc));
	DNPRINTF(MPII_D_MISC, "%s:  flags: 0x08%x\n", DEVNAME(sc),
	    le32toh(pg.flags));
	DNPRINTF(MPII_D_MISC, "%s:  coalescing_timeout: %d\n", DEVNAME(sc),
	    le32toh(pg.coalescing_timeout));
	DNPRINTF(MPII_D_MISC, "%s:  coalescing_depth: %d pci_slot_num: %d\n",
	    DEVNAME(sc), pg.coalescing_timeout, pg.pci_slot_num);

	if (!ISSET(le32toh(pg.flags), MPII_CFG_IOC_1_REPLY_COALESCING))
		return (0);

	CLR(pg.flags, htole32(MPII_CFG_IOC_1_REPLY_COALESCING));
	if (mpii_cfg_page(sc, 0, &hdr, 0, &pg, sizeof(pg)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to clear coalescing\n",
		    DEVNAME(sc));
		return (1);
	}

	return (0);
}

#define MPII_EVENT_MASKALL(enq)		do {			\
		enq->event_masks[0] = 0xffffffff;		\
		enq->event_masks[1] = 0xffffffff;		\
		enq->event_masks[2] = 0xffffffff;		\
		enq->event_masks[3] = 0xffffffff;		\
	} while (0)

#define MPII_EVENT_UNMASK(enq, evt)	do {			\
		enq->event_masks[evt / 32] &=			\
		    htole32(~(1 << (evt % 32)));		\
	} while (0)

static int
mpii_eventnotify(struct mpii_softc *sc)
{
	struct mpii_msg_event_request		*enq;
	struct mpii_ccb				*ccb;

	ccb = mpii_get_ccb(sc, 0);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_eventnotify ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpii_eventnotify_done;
	enq = ccb->ccb_cmd;

	enq->function = MPII_FUNCTION_EVENT_NOTIFICATION;

	/*
	 * Enable reporting of the following events:
	 *
	 * MPII_EVENT_SAS_DISCOVERY
	 * MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST
	 * MPII_EVENT_SAS_DEVICE_STATUS_CHANGE
	 * MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE
	 * MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST
	 * MPII_EVENT_IR_VOLUME
	 * MPII_EVENT_IR_PHYSICAL_DISK
	 * MPII_EVENT_IR_OPERATION_STATUS
	 */

	MPII_EVENT_MASKALL(enq);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_SAS_DISCOVERY);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_SAS_DEVICE_STATUS_CHANGE);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_IR_VOLUME);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_IR_PHYSICAL_DISK);
	MPII_EVENT_UNMASK(enq, MPII_EVENT_IR_OPERATION_STATUS);

	mpii_start(sc, ccb);

	return (0);
}

static void
mpii_eventnotify_done(struct mpii_ccb *ccb)
{
	struct mpii_softc			*sc = ccb->ccb_sc;
	struct mpii_rcb				*rcb = ccb->ccb_rcb;

	DNPRINTF(MPII_D_EVT, "%s: mpii_eventnotify_done\n", DEVNAME(sc));

	mpii_put_ccb(sc, ccb);
	mpii_event_process(sc, rcb);
}

static void
mpii_event_raid(struct mpii_softc *sc, struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_ir_cfg_change_list	*ccl;
	struct mpii_evt_ir_cfg_element		*ce;
	struct mpii_device			*dev;
	u_int16_t				type;
	int					i;

	ccl = (struct mpii_evt_ir_cfg_change_list *)(enp + 1);

	if (ccl->num_elements == 0)
		return;
	if (ISSET(le32toh(ccl->flags), MPII_EVT_IR_CFG_CHANGE_LIST_FOREIGN))
		/* bail on foreign configurations */
		return;

	ce = (struct mpii_evt_ir_cfg_element *)(ccl + 1);

	for (i = 0; i < ccl->num_elements; i++, ce++) {
		type = (le16toh(ce->element_flags) &
		    MPII_EVT_IR_CFG_ELEMENT_TYPE_MASK);

		switch (type) {
		case MPII_EVT_IR_CFG_ELEMENT_TYPE_VOLUME:
			switch (ce->reason_code) {
			case MPII_EVT_IR_CFG_ELEMENT_RC_ADDED:
			case MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_CREATED:
				if (mpii_find_dev(sc,
				    le16toh(ce->vol_dev_handle))) {
					printf("%s: device %#x is already "
					    "configured\n", DEVNAME(sc),
					    le16toh(ce->vol_dev_handle));
					break;
				}
				dev = malloc(sizeof(*dev), M_DEVBUF,
				    M_NOWAIT | M_ZERO);
				if (!dev) {
					printf("%s: failed to allocate a "
				    	    "device structure\n", DEVNAME(sc));
					break;
				}
				SET(dev->flags, MPII_DF_VOLUME);
				dev->slot = sc->sc_vd_id_low;
				dev->dev_handle = le16toh(ce->vol_dev_handle);
				if (mpii_insert_dev(sc, dev)) {
					free(dev, M_DEVBUF);
					break;
				}
				mpii_cache_enable(sc, dev);
				sc->sc_vd_count++;
				break;
			case MPII_EVT_IR_CFG_ELEMENT_RC_REMOVED:
			case MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_DELETED:
				if (!(dev = mpii_find_dev(sc,
				    le16toh(ce->vol_dev_handle))))
					break;
				mpii_remove_dev(sc, dev);
				sc->sc_vd_count--;
				break;
			}
			break;
		case MPII_EVT_IR_CFG_ELEMENT_TYPE_VOLUME_DISK:
			if (ce->reason_code ==
			    MPII_EVT_IR_CFG_ELEMENT_RC_PD_CREATED ||
			    ce->reason_code ==
			    MPII_EVT_IR_CFG_ELEMENT_RC_HIDE) {
				/* there should be an underlying sas drive */
				if (!(dev = mpii_find_dev(sc,
				    le16toh(ce->phys_disk_dev_handle))))
					break;
				/* promoted from a hot spare? */
				CLR(dev->flags, MPII_DF_HOT_SPARE);
				SET(dev->flags, MPII_DF_VOLUME_DISK |
				    MPII_DF_HIDDEN);
			}
			break;
		case MPII_EVT_IR_CFG_ELEMENT_TYPE_HOT_SPARE:
			if (ce->reason_code ==
			    MPII_EVT_IR_CFG_ELEMENT_RC_HIDE) {
				/* there should be an underlying sas drive */
				if (!(dev = mpii_find_dev(sc,
				    le16toh(ce->phys_disk_dev_handle))))
					break;
				SET(dev->flags, MPII_DF_HOT_SPARE |
				    MPII_DF_HIDDEN);
			}
			break;
		}
	}
}

static void
mpii_event_sas(struct mpii_softc *sc, struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_sas_tcl		*tcl;
	struct mpii_evt_phy_entry	*pe;
	struct mpii_device		*dev;
	int				i;

	tcl = (struct mpii_evt_sas_tcl *)(enp + 1);

	if (tcl->num_entries == 0)
		return;

	pe = (struct mpii_evt_phy_entry *)(tcl + 1);

	for (i = 0; i < tcl->num_entries; i++, pe++) {
		switch (pe->phy_status & MPII_EVENT_SAS_TOPO_PS_RC_MASK) {
		case MPII_EVENT_SAS_TOPO_PS_RC_ADDED:
			if (mpii_find_dev(sc, le16toh(pe->dev_handle))) {
				printf("%s: device %#x is already "
				    "configured\n", DEVNAME(sc),
				    le16toh(pe->dev_handle));
				break;
			}
			dev = malloc(sizeof(*dev), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (!dev) {
				printf("%s: failed to allocate a "
				    "device structure\n", DEVNAME(sc));
				break;
			}
			dev->slot = sc->sc_pd_id_start + tcl->start_phy_num + i;
			dev->dev_handle = le16toh(pe->dev_handle);
			dev->phy_num = tcl->start_phy_num + i;
			if (tcl->enclosure_handle)
				dev->physical_port = tcl->physical_port;
			dev->enclosure = le16toh(tcl->enclosure_handle);
			dev->expander = le16toh(tcl->expander_handle);
			if (mpii_insert_dev(sc, dev)) {
				free(dev, M_DEVBUF);
				break;
			}
			break;
		case MPII_EVENT_SAS_TOPO_PS_RC_MISSING:
			if (!(dev = mpii_find_dev(sc,
			    le16toh(pe->dev_handle))))
				break;
			mpii_remove_dev(sc, dev);
#if 0
			if (sc->sc_scsibus) {
				SET(dev->flags, MPII_DF_DETACH);
				scsi_activate(sc->sc_scsibus, dev->slot, -1,
				    DVACT_DEACTIVATE);
				if (scsi_task(mpii_event_defer, sc,
				    dev, 0) != 0)
					printf("%s: unable to run device "
					    "detachment routine\n",
					    DEVNAME(sc));
			}
#else
			mpii_event_defer(sc, dev);
#endif /* XXX */
			break;
		}
	}
}

static void
mpii_event_process(struct mpii_softc *sc, struct mpii_rcb *rcb)
{
	struct mpii_msg_event_reply		*enp;

	enp = (struct mpii_msg_event_reply *)rcb->rcb_reply;

	DNPRINTF(MPII_D_EVT, "%s: mpii_event_process: %#x\n", DEVNAME(sc),
	    le32toh(enp->event));

	switch (le32toh(enp->event)) {
	case MPII_EVENT_EVENT_CHANGE:
		/* should be properly ignored */
		break;
	case MPII_EVENT_SAS_DISCOVERY: {
		struct mpii_evt_sas_discovery	*esd =
		    (struct mpii_evt_sas_discovery *)(enp + 1);

		if (esd->reason_code ==
		    MPII_EVENT_SAS_DISC_REASON_CODE_COMPLETED &&
		    esd->discovery_status != 0)
			printf("%s: sas discovery completed with status %#x\n",
			    DEVNAME(sc), esd->discovery_status);
		}
		break;
	case MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		mpii_event_sas(sc, enp);
		break;
	case MPII_EVENT_SAS_DEVICE_STATUS_CHANGE:
		break;
	case MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
		break;
	case MPII_EVENT_IR_VOLUME: {
		struct mpii_evt_ir_volume	*evd =
		    (struct mpii_evt_ir_volume *)(enp + 1);
		struct mpii_device		*dev;
#if NBIO > 0
		const char *vol_states[] = {
			BIOC_SVINVALID_S,
			BIOC_SVOFFLINE_S,
			BIOC_SVBUILDING_S,
			BIOC_SVONLINE_S,
			BIOC_SVDEGRADED_S,
			BIOC_SVONLINE_S,
		};
#endif

		if (cold)
			break;
		if (!(dev = mpii_find_dev(sc, le16toh(evd->vol_dev_handle))))
			break;
#if NBIO > 0
		if (evd->reason_code == MPII_EVENT_IR_VOL_RC_STATE_CHANGED)
			printf("%s: volume %d state changed from %s to %s\n",
			    DEVNAME(sc), dev->slot - sc->sc_vd_id_low,
			    vol_states[evd->prev_value],
			    vol_states[evd->new_value]);
#endif
		if (evd->reason_code == MPII_EVENT_IR_VOL_RC_STATUS_CHANGED &&
		    ISSET(evd->new_value, MPII_CFG_RAID_VOL_0_STATUS_RESYNC) &&
		    !ISSET(evd->prev_value, MPII_CFG_RAID_VOL_0_STATUS_RESYNC))
			printf("%s: started resync on a volume %d\n",
			    DEVNAME(sc), dev->slot - sc->sc_vd_id_low);
		}
		break;
	case MPII_EVENT_IR_PHYSICAL_DISK:
		break;
	case MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST:
		mpii_event_raid(sc, enp);
		break;
	case MPII_EVENT_IR_OPERATION_STATUS: {
		struct mpii_evt_ir_status	*evs =
		    (struct mpii_evt_ir_status *)(enp + 1);
		struct mpii_device		*dev;

		if (!(dev = mpii_find_dev(sc, le16toh(evs->vol_dev_handle))))
			break;
		if (evs->operation == MPII_EVENT_IR_RAIDOP_RESYNC)
			dev->percent = evs->percent;
		break;
		}
	default:
		DNPRINTF(MPII_D_EVT, "%s:  unhandled event 0x%02x\n",
		    DEVNAME(sc), le32toh(enp->event));
	}

	if (enp->ack_required)
		workqueue_enqueue(sc->sc_ssb_evt_ackwk, &rcb->u.rcb_wk, NULL);
	else
		mpii_push_reply(sc, rcb);
}

static void
mpii_event_defer(void *xsc, void *arg)
{
	struct mpii_softc	*sc = xsc;
	struct mpii_device	*dev = arg;

	if (ISSET(dev->flags, MPII_DF_DETACH)) {
		mpii_sas_remove_device(sc, dev->dev_handle);
#if 0
		if (!ISSET(dev->flags, MPII_DF_HIDDEN)) {
			scsi_detach_target(sc->sc_scsibus, dev->slot,
			    DETACH_FORCE);
		}
#endif /* XXX */
		free(dev, M_DEVBUF);

	} else if (ISSET(dev->flags, MPII_DF_ATTACH)) {
		CLR(dev->flags, MPII_DF_ATTACH);
#if 0
		if (!ISSET(dev->flags, MPII_DF_HIDDEN))
			scsi_probe_target(sc->sc_scsibus, dev->slot);
#endif /* XXX */
	}
}

static void
mpii_sas_remove_device(struct mpii_softc *sc, u_int16_t handle)
{
 	struct mpii_msg_scsi_task_request	*stq;
	struct mpii_msg_sas_oper_request	*soq;
	struct mpii_ccb				*ccb;

	ccb = mpii_get_ccb(sc, 0);
	if (ccb == NULL)
		return;

	stq = ccb->ccb_cmd;
	stq->function = MPII_FUNCTION_SCSI_TASK_MGMT;
	stq->task_type = MPII_SCSI_TASK_TARGET_RESET;
	stq->dev_handle = htole16(handle);

	ccb->ccb_done = mpii_empty_done;
	mpii_wait(sc, ccb);

	if (ccb->ccb_rcb != NULL)
		mpii_push_reply(sc, ccb->ccb_rcb);

	/* reuse a ccb */
	ccb->ccb_state = MPII_CCB_READY;
	ccb->ccb_rcb = NULL;

	soq = ccb->ccb_cmd;
	bzero(soq, sizeof(*soq));
	soq->function = MPII_FUNCTION_SAS_IO_UNIT_CONTROL;
	soq->operation = MPII_SAS_OP_REMOVE_DEVICE;
	soq->dev_handle = htole16(handle);

	ccb->ccb_done = mpii_empty_done;
	mpii_wait(sc, ccb);
	if (ccb->ccb_rcb != NULL)
		mpii_push_reply(sc, ccb->ccb_rcb);
}

static int
mpii_get_ioc_pg8(struct mpii_softc *sc)
{
	struct mpii_cfg_hdr	hdr;
	struct mpii_cfg_ioc_pg8	*page;
	size_t			pagelen;
	u_int16_t		flags;
	int			pad = 0, rv = 0;

	DNPRINTF(MPII_D_RAID, "%s: mpii_get_ioc_pg8\n", DEVNAME(sc));

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_IOC, 8, 0,
	    &hdr) != 0) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_ioc_pg8 unable to fetch "
		    "header for IOC page 8\n", DEVNAME(sc));
		return (1);
	}

	pagelen = hdr.page_length * 4; /* dwords to bytes */

	page = malloc(pagelen, M_TEMP, M_NOWAIT);
	if (page == NULL) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_ioc_pg8 unable to allocate "
		    "space for ioc config page 8\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_cfg_page(sc, 0, &hdr, 1, page, pagelen) != 0) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_raid unable to fetch IOC "
		    "page 8\n", DEVNAME(sc));
		rv = 1;
		goto out;
	}

	DNPRINTF(MPII_D_CFG, "%s:  numdevsperenclosure: 0x%02x\n", DEVNAME(sc),
	    page->num_devs_per_enclosure);
	DNPRINTF(MPII_D_CFG, "%s:  maxpersistententries: 0x%04x "
	    "maxnumphysicalmappedids: 0x%04x\n", DEVNAME(sc),
	    le16toh(page->max_persistent_entries),
	    le16toh(page->max_num_physical_mapped_ids));
	DNPRINTF(MPII_D_CFG, "%s:  flags: 0x%04x\n", DEVNAME(sc),
	    le16toh(page->flags));
	DNPRINTF(MPII_D_CFG, "%s:  irvolumemappingflags: 0x%04x\n",
	    DEVNAME(sc), le16toh(page->ir_volume_mapping_flags));

	if (page->flags & MPII_IOC_PG8_FLAGS_RESERVED_TARGETID_0)
		pad = 1;

	flags = page->ir_volume_mapping_flags &
	    MPII_IOC_PG8_IRFLAGS_VOLUME_MAPPING_MODE_MASK;
	if (ISSET(sc->sc_flags, MPII_F_RAID)) {
		if (flags == MPII_IOC_PG8_IRFLAGS_LOW_VOLUME_MAPPING) {
			sc->sc_vd_id_low += pad;
			pad = sc->sc_max_volumes; /* for sc_pd_id_start */
		} else
			sc->sc_vd_id_low = sc->sc_max_devices -
			    sc->sc_max_volumes;
	}

	sc->sc_pd_id_start += pad;

	DNPRINTF(MPII_D_MAP, "%s: mpii_get_ioc_pg8 mapping: sc_pd_id_start: %d "
	    "sc_vd_id_low: %d sc_max_volumes: %d\n", DEVNAME(sc),
	    sc->sc_pd_id_start, sc->sc_vd_id_low, sc->sc_max_volumes);

out:
	free(page, M_TEMP);

	return(rv);
}

static int
mpii_req_cfg_header(struct mpii_softc *sc, u_int8_t type, u_int8_t number,
    u_int32_t address, int flags, void *p)
{
	struct mpii_msg_config_request		*cq;
	struct mpii_msg_config_reply		*cp;
	struct mpii_cfg_hdr	*hdr = p;
	struct mpii_ccb		*ccb;
	struct mpii_ecfg_hdr	*ehdr = p;
	int			etype = 0;
	int			rv = 0;

	DNPRINTF(MPII_D_MISC, "%s: mpii_req_cfg_header type: %#x number: %x "
	    "address: 0x%08x flags: 0x%x\n", DEVNAME(sc), type, number,
	    address, flags);

	ccb = mpii_get_ccb(sc, ISSET(flags, MPII_PG_POLL) ? MPII_NOSLEEP : 0);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	if (ISSET(flags, MPII_PG_EXTENDED)) {
		etype = type;
		type = MPII_CONFIG_REQ_PAGE_TYPE_EXTENDED;
	}

	cq = ccb->ccb_cmd;

	cq->function = MPII_FUNCTION_CONFIG;

	cq->action = MPII_CONFIG_REQ_ACTION_PAGE_HEADER;

	cq->config_header.page_number = number;
	cq->config_header.page_type = type;
	cq->ext_page_type = etype;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
	    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL);

	ccb->ccb_done = mpii_empty_done;
	if (ISSET(flags, MPII_PG_POLL)) {
		if (mpii_poll(sc, ccb) != 0) {
			DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header poll\n",
			    DEVNAME(sc));
			return (1);
		}
	} else
		mpii_wait(sc, ccb);

	if (ccb->ccb_rcb == NULL) {
		mpii_put_ccb(sc, ccb);
		return (1);
	}
	cp = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_MISC, "%s:  action: 0x%02x sgl_flags: 0x%02x "
	    "msg_length: %d function: 0x%02x\n", DEVNAME(sc), cp->action,
	    cp->sgl_flags, cp->msg_length, cp->function);
	DNPRINTF(MPII_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    le16toh(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    cp->vp_id, cp->vf_id);	
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    le16toh(cp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(cp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version,
	    cp->config_header.page_length,
	    cp->config_header.page_number,
	    cp->config_header.page_type);

	if (le16toh(cp->ioc_status) != MPII_IOCSTATUS_SUCCESS)
		rv = 1;
	else if (ISSET(flags, MPII_PG_EXTENDED)) {
		bzero(ehdr, sizeof(*ehdr));
		ehdr->page_version = cp->config_header.page_version;
		ehdr->page_number = cp->config_header.page_number;
		ehdr->page_type = cp->config_header.page_type;
		ehdr->ext_page_length = cp->ext_page_length;
		ehdr->ext_page_type = cp->ext_page_type;
	} else
		*hdr = cp->config_header;

	mpii_push_reply(sc, ccb->ccb_rcb);
	mpii_put_ccb(sc, ccb);

	return (rv);
}

static int
mpii_req_cfg_page(struct mpii_softc *sc, u_int32_t address, int flags,
    void *p, int read, void *page, size_t len)
{
	struct mpii_msg_config_request		*cq;
	struct mpii_msg_config_reply		*cp;
	struct mpii_cfg_hdr	*hdr = p;
	struct mpii_ccb		*ccb;
	struct mpii_ecfg_hdr	*ehdr = p;
	u_int64_t		dva;
	char			*kva;
	int			page_length;
	int			rv = 0;

	DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_page address: %d read: %d "
	    "type: %x\n", DEVNAME(sc), address, read, hdr->page_type);

	page_length = ISSET(flags, MPII_PG_EXTENDED) ?
	    le16toh(ehdr->ext_page_length) : hdr->page_length;

	if (len > MPII_REQUEST_SIZE - sizeof(struct mpii_msg_config_request) ||
    	    len < page_length * 4)
		return (1);

	ccb = mpii_get_ccb(sc,
	    ISSET(flags, MPII_PG_POLL) ? MPII_NOSLEEP : 0);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_page ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	cq = ccb->ccb_cmd;

	cq->function = MPII_FUNCTION_CONFIG;

	cq->action = (read ? MPII_CONFIG_REQ_ACTION_PAGE_READ_CURRENT :
	    MPII_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT);

	if (ISSET(flags, MPII_PG_EXTENDED)) {
		cq->config_header.page_version = ehdr->page_version;
		cq->config_header.page_number = ehdr->page_number;
		cq->config_header.page_type = ehdr->page_type;
		cq->ext_page_len = ehdr->ext_page_length;
		cq->ext_page_type = ehdr->ext_page_type;
	} else
		cq->config_header = *hdr;
	cq->config_header.page_type &= MPII_CONFIG_REQ_PAGE_TYPE_MASK;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
	    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL |
	    MPII_SGE_FL_SIZE_64 | (page_length * 4) |
	    (read ? MPII_SGE_FL_DIR_IN : MPII_SGE_FL_DIR_OUT));

	/* bounce the page via the request space to avoid more bus_dma games */
	dva = ccb->ccb_cmd_dva + sizeof(struct mpii_msg_config_request);

	cq->page_buffer.sg_hi_addr = htole32((u_int32_t)(dva >> 32));
	cq->page_buffer.sg_lo_addr = htole32((u_int32_t)dva);

	kva = ccb->ccb_cmd;
	kva += sizeof(struct mpii_msg_config_request);

	if (!read)
		bcopy(page, kva, len);

	ccb->ccb_done = mpii_empty_done;
	if (ISSET(flags, MPII_PG_POLL)) {
		if (mpii_poll(sc, ccb) != 0) {
			DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header poll\n",
			    DEVNAME(sc));
			return (1);
		}
	} else
		mpii_wait(sc, ccb);

	if (ccb->ccb_rcb == NULL) {
		mpii_put_ccb(sc, ccb);
		return (1);
	}
	cp = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_MISC, "%s:  action: 0x%02x "
	    "msg_length: %d function: 0x%02x\n", DEVNAME(sc), cp->action,
	    cp->msg_length, cp->function);
	DNPRINTF(MPII_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    le16toh(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    cp->vp_id, cp->vf_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    le16toh(cp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(cp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version,
	    cp->config_header.page_length,
	    cp->config_header.page_number,
	    cp->config_header.page_type);
	
	if (le16toh(cp->ioc_status) != MPII_IOCSTATUS_SUCCESS)
		rv = 1;
	else if (read)
		bcopy(kva, page, len);

	mpii_push_reply(sc, ccb->ccb_rcb);
	mpii_put_ccb(sc, ccb);

	return (rv);
}

static struct mpii_rcb *
mpii_reply(struct mpii_softc *sc, struct mpii_reply_descr *rdp)
{
	struct mpii_rcb		*rcb = NULL;
	u_int32_t		rfid;

	DNPRINTF(MPII_D_INTR, "%s: mpii_reply\n", DEVNAME(sc));

	if ((rdp->reply_flags & MPII_REPLY_DESCR_TYPE_MASK) ==
	    MPII_REPLY_DESCR_ADDRESS_REPLY) {
		rfid = (le32toh(rdp->frame_addr) -
		    (u_int32_t)MPII_DMA_DVA(sc->sc_replies)) / MPII_REPLY_SIZE;

		bus_dmamap_sync(sc->sc_dmat,
		    MPII_DMA_MAP(sc->sc_replies), MPII_REPLY_SIZE * rfid,
		    MPII_REPLY_SIZE, BUS_DMASYNC_POSTREAD);

		rcb = &sc->sc_rcbs[rfid];
	}

	memset(rdp, 0xff, sizeof(*rdp));

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq),
	    8 * sc->sc_reply_post_host_index, 8,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	return (rcb);
}

static struct mpii_dmamem *
mpii_dmamem_alloc(struct mpii_softc *sc, size_t size)
{
	struct mpii_dmamem	*mdm;
	int			nsegs;

	mdm = malloc(sizeof(*mdm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mdm == NULL)
	return (NULL);

	mdm->mdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mdm->mdm_map) != 0)
		goto mdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mdm->mdm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0) goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mdm->mdm_seg, nsegs, size,
	    &mdm->mdm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mdm->mdm_map, mdm->mdm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	DNPRINTF(MPII_D_MEM,
	    "  kva: %p  dva: 0x%" PRIx64 "  map: %p  size: %" PRId64 "\n",
	    mdm->mdm_kva, (uint64_t)mdm->mdm_map->dm_segs[0].ds_addr,
	    mdm->mdm_map, (uint64_t)size);

	bzero(mdm->mdm_kva, size);

	return (mdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
mdmfree:
	free(mdm, M_DEVBUF);

	return (NULL);
}

static void
mpii_dmamem_free(struct mpii_softc *sc, struct mpii_dmamem *mdm)
{
	DNPRINTF(MPII_D_MEM, "%s: mpii_dmamem_free %p\n", DEVNAME(sc), mdm);

	bus_dmamap_unload(sc->sc_dmat, mdm->mdm_map);
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, mdm->mdm_size);
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
	free(mdm, M_DEVBUF);
}

static int
mpii_alloc_dev(struct mpii_softc *sc)
{
	sc->sc_devs = malloc(sc->sc_max_devices *
	    sizeof(struct mpii_device *), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_devs == NULL)
		return (1);
	return (0);
}

static int
mpii_insert_dev(struct mpii_softc *sc, struct mpii_device *dev)
{
	int slot = dev->slot; 	/* initial hint */

	if (!dev || slot < 0)
		return (1);
	while (slot < sc->sc_max_devices && sc->sc_devs[slot] != NULL)
		slot++;
	if (slot >= sc->sc_max_devices)
		return (1);
	dev->slot = slot;
	sc->sc_devs[slot] = dev;
	return (0);
}

static int
mpii_remove_dev(struct mpii_softc *sc, struct mpii_device *dev)
{
	int			i;

	if (!dev)
		return (1);
	for (i = 0; i < sc->sc_max_devices;  i++)
		if (sc->sc_devs[i] &&
		    sc->sc_devs[i]->dev_handle == dev->dev_handle) {
			sc->sc_devs[i] = NULL;
			return (0);
		}
	return (1);
}

static struct mpii_device *
mpii_find_dev(struct mpii_softc *sc, u_int16_t handle)
{
	int			i;

	for (i = 0; i < sc->sc_max_devices;  i++)
		if (sc->sc_devs[i] && sc->sc_devs[i]->dev_handle == handle)
			return (sc->sc_devs[i]);
	return (NULL);
}

static int
mpii_alloc_ccbs(struct mpii_softc *sc)
{
	struct mpii_ccb		*ccb;
	u_int8_t		*cmd;
	int			i;

	SIMPLEQ_INIT(&sc->sc_ccb_free);

	sc->sc_ccbs = malloc(sizeof(*ccb) * (sc->sc_request_depth-1),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_ccbs == NULL) {
		printf("%s: unable to allocate ccbs\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_requests = mpii_dmamem_alloc(sc,
	    MPII_REQUEST_SIZE * sc->sc_request_depth);
	if (sc->sc_requests == NULL) {
		printf("%s: unable to allocate ccb dmamem\n", DEVNAME(sc));
		goto free_ccbs;
	}
	cmd = MPII_DMA_KVA(sc->sc_requests);
	bzero(cmd, MPII_REQUEST_SIZE * sc->sc_request_depth);

	/*
	 * we have sc->sc_request_depth system request message
	 * frames, but smid zero cannot be used. so we then
	 * have (sc->sc_request_depth - 1) number of ccbs
	 */
	for (i = 1; i < sc->sc_request_depth; i++) {
		ccb = &sc->sc_ccbs[i - 1];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS,
		    sc->sc_max_sgl_len, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dma map\n", DEVNAME(sc));
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_smid = i;
		ccb->ccb_offset = MPII_REQUEST_SIZE * i;

		ccb->ccb_cmd = &cmd[ccb->ccb_offset];
		ccb->ccb_cmd_dva = (u_int32_t)MPII_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_offset;

		DNPRINTF(MPII_D_CCB, "%s: mpii_alloc_ccbs(%d) ccb: %p map: %p "
		    "sc: %p smid: %#x offs: %#" PRIx64 " cmd: %#" PRIx64 " dva: %#" PRIx64 "\n",
		    DEVNAME(sc), i, ccb, ccb->ccb_dmamap, ccb->ccb_sc,
		    ccb->ccb_smid, (uint64_t)ccb->ccb_offset,
		    (uint64_t)ccb->ccb_cmd, (uint64_t)ccb->ccb_cmd_dva);

		mpii_put_ccb(sc, ccb);
	}

	return (0);

free_maps:
	while ((ccb = mpii_get_ccb(sc, MPII_NOSLEEP)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	mpii_dmamem_free(sc, sc->sc_requests);
free_ccbs:
	free(sc->sc_ccbs, M_DEVBUF);

	return (1);
}

static void
mpii_put_ccb(struct mpii_softc *sc, struct mpii_ccb *ccb)
{
	KASSERT(ccb->ccb_sc == sc);
	DNPRINTF(MPII_D_CCB, "%s: mpii_put_ccb %p\n", DEVNAME(sc), ccb);

	ccb->ccb_state = MPII_CCB_FREE;
	ccb->ccb_cookie = NULL;
	ccb->ccb_done = NULL;
	ccb->ccb_rcb = NULL;
	bzero(ccb->ccb_cmd, MPII_REQUEST_SIZE);

	mutex_enter(&sc->sc_ccb_free_mtx);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_free, ccb, u.ccb_link);
	cv_signal(&sc->sc_ccb_free_cv);
	mutex_exit(&sc->sc_ccb_free_mtx);
}

static struct mpii_ccb *
mpii_get_ccb(struct mpii_softc *sc, int flags)
{
	struct mpii_ccb		*ccb;

	mutex_enter(&sc->sc_ccb_free_mtx);
	while ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_free)) == NULL) {
		if (flags & MPII_NOSLEEP)
			break;
		cv_wait(&sc->sc_ccb_free_cv, &sc->sc_ccb_free_mtx);
	}
		
	if (ccb != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_free, u.ccb_link);
		ccb->ccb_state = MPII_CCB_READY;
		KASSERT(ccb->ccb_sc == sc);
	}
	mutex_exit(&sc->sc_ccb_free_mtx);

	DNPRINTF(MPII_D_CCB, "%s: mpii_get_ccb %p\n", DEVNAME(sc), ccb);

	return (ccb);
}

static int
mpii_alloc_replies(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s: mpii_alloc_replies\n", DEVNAME(sc));

	sc->sc_rcbs = malloc(sc->sc_num_reply_frames * sizeof(struct mpii_rcb),
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_rcbs == NULL)
		return (1);

	sc->sc_replies = mpii_dmamem_alloc(sc, MPII_REPLY_SIZE *
	    sc->sc_num_reply_frames);
	if (sc->sc_replies == NULL) {
		free(sc->sc_rcbs, M_DEVBUF);
		return (1);
	}

	return (0);
}

static void
mpii_push_replies(struct mpii_softc *sc)
{
	struct mpii_rcb		*rcb;
	char			*kva = MPII_DMA_KVA(sc->sc_replies);
	int			i;

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_replies),
	    0, MPII_REPLY_SIZE * sc->sc_num_reply_frames, BUS_DMASYNC_PREREAD);

	for (i = 0; i < sc->sc_num_reply_frames; i++) {
		rcb = &sc->sc_rcbs[i];

		rcb->rcb_reply = kva + MPII_REPLY_SIZE * i;
		rcb->rcb_reply_dva = (u_int32_t)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i;
		mpii_push_reply(sc, rcb);
	}
}

static void
mpii_start(struct mpii_softc *sc, struct mpii_ccb *ccb)
{
	struct mpii_request_header	*rhp;
	struct mpii_request_descr	descr;
	u_int32_t			*rdp = (u_int32_t *)&descr;

	DNPRINTF(MPII_D_RW, "%s: mpii_start %#" PRIx64 "\n", DEVNAME(sc),
	    (uint64_t)ccb->ccb_cmd_dva);

	rhp = ccb->ccb_cmd;

	bzero(&descr, sizeof(descr));

	switch (rhp->function) {
	case MPII_FUNCTION_SCSI_IO_REQUEST:
		descr.request_flags = MPII_REQ_DESCR_SCSI_IO;
		descr.dev_handle = htole16(ccb->ccb_dev_handle);
		break;
	case MPII_FUNCTION_SCSI_TASK_MGMT:
		descr.request_flags = MPII_REQ_DESCR_HIGH_PRIORITY;
		break;
	default:
		descr.request_flags = MPII_REQ_DESCR_DEFAULT;
	}

	descr.vf_id = sc->sc_vf_id;
	descr.smid = htole16(ccb->ccb_smid);

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_requests),
	    ccb->ccb_offset, MPII_REQUEST_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	ccb->ccb_state = MPII_CCB_QUEUED;

	DNPRINTF(MPII_D_RW, "%s:   MPII_REQ_DESCR_POST_LOW (0x%08x) write "
	    "0x%08x\n", DEVNAME(sc), MPII_REQ_DESCR_POST_LOW, *rdp);

	DNPRINTF(MPII_D_RW, "%s:   MPII_REQ_DESCR_POST_HIGH (0x%08x) write "
	    "0x%08x\n", DEVNAME(sc), MPII_REQ_DESCR_POST_HIGH, *(rdp+1));

	mutex_enter(&sc->sc_req_mtx);
	mpii_write(sc, MPII_REQ_DESCR_POST_LOW, htole32(*rdp));
	mpii_write(sc, MPII_REQ_DESCR_POST_HIGH, htole32(*(rdp+1)));
	mutex_exit(&sc->sc_req_mtx);
}

static int
mpii_poll(struct mpii_softc *sc, struct mpii_ccb *ccb)
{
	void				(*done)(struct mpii_ccb *);
	void				*cookie;
	int				rv = 1;

	DNPRINTF(MPII_D_INTR, "%s: mpii_complete\n", DEVNAME(sc));

	done = ccb->ccb_done;
	cookie = ccb->ccb_cookie;

	ccb->ccb_done = mpii_poll_done;
	ccb->ccb_cookie = &rv;

	mpii_start(sc, ccb);

	while (rv == 1) {
		/* avoid excessive polling */
		if (mpii_reply_waiting(sc))
			mpii_intr(sc);
		else
			delay(10);
	}

	ccb->ccb_cookie = cookie;
	done(ccb);

	return (0);
}

static void
mpii_poll_done(struct mpii_ccb *ccb)
{
	int				*rv = ccb->ccb_cookie;

	*rv = 0;
}

static int
mpii_alloc_queues(struct mpii_softc *sc)
{
	u_int32_t		*kva;
	u_int64_t		*kva64;
	int			i;
	
	DNPRINTF(MPII_D_MISC, "%s: mpii_alloc_queues\n", DEVNAME(sc));

	sc->sc_reply_freeq = mpii_dmamem_alloc(sc,
	    sc->sc_reply_free_qdepth * 4);
	if (sc->sc_reply_freeq == NULL)
		return (1);
	
	kva = MPII_DMA_KVA(sc->sc_reply_freeq);
	for (i = 0; i < sc->sc_num_reply_frames; i++) {
		kva[i] = (u_int32_t)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i;

		DNPRINTF(MPII_D_MISC, "%s:   %d:  %p = 0x%08x\n",
		    DEVNAME(sc), i,
		    &kva[i], (u_int)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i);
	}

	sc->sc_reply_postq =
	    mpii_dmamem_alloc(sc, sc->sc_reply_post_qdepth * 8);
	if (sc->sc_reply_postq == NULL)
		goto free_reply_freeq;
	sc->sc_reply_postq_kva = MPII_DMA_KVA(sc->sc_reply_postq);
	
	DNPRINTF(MPII_D_MISC, "%s:  populating reply post descriptor queue\n",
	    DEVNAME(sc));
	kva64 = (u_int64_t *)MPII_DMA_KVA(sc->sc_reply_postq);
	for (i = 0; i < sc->sc_reply_post_qdepth; i++) {
		kva64[i] = 0xffffffffffffffffllu;
		DNPRINTF(MPII_D_MISC, "%s:    %d:  %p = 0x%" PRIx64 "\n",
		    DEVNAME(sc), i, &kva64[i], kva64[i]);
	}

	return (0);

free_reply_freeq:

	mpii_dmamem_free(sc, sc->sc_reply_freeq);
	return (1);
}

static void
mpii_init_queues(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s:  mpii_init_queues\n", DEVNAME(sc));

	sc->sc_reply_free_host_index = sc->sc_reply_free_qdepth - 1;
	sc->sc_reply_post_host_index = 0;
	mpii_write_reply_free(sc, sc->sc_reply_free_host_index);
	mpii_write_reply_post(sc, sc->sc_reply_post_host_index);
}

static void
mpii_wait(struct mpii_softc *sc, struct mpii_ccb *ccb)
{
	struct mpii_ccb_wait	mpii_ccb_wait;
	void			(*done)(struct mpii_ccb *);
	void			*cookie;

	done = ccb->ccb_done;
	cookie = ccb->ccb_cookie;

	ccb->ccb_done = mpii_wait_done;
	ccb->ccb_cookie = &mpii_ccb_wait;

	mutex_init(&mpii_ccb_wait.mpii_ccbw_mtx, MUTEX_DEFAULT, IPL_BIO);
	cv_init(&mpii_ccb_wait.mpii_ccbw_cv, "mpii_wait");

	/* XXX this will wait forever for the ccb to complete */

	mpii_start(sc, ccb);

	mutex_enter(&mpii_ccb_wait.mpii_ccbw_mtx);
	while (ccb->ccb_cookie != NULL) {
		cv_wait(&mpii_ccb_wait.mpii_ccbw_cv,
		    &mpii_ccb_wait.mpii_ccbw_mtx);
	}
	mutex_exit(&mpii_ccb_wait.mpii_ccbw_mtx);
	mutex_destroy(&mpii_ccb_wait.mpii_ccbw_mtx);
	cv_destroy(&mpii_ccb_wait.mpii_ccbw_cv);

	ccb->ccb_cookie = cookie;
	done(ccb);
}

static void
mpii_wait_done(struct mpii_ccb *ccb)
{
	struct mpii_ccb_wait	*mpii_ccb_waitp = ccb->ccb_cookie;

	mutex_enter(&mpii_ccb_waitp->mpii_ccbw_mtx);
	ccb->ccb_cookie = NULL;
	cv_signal(&mpii_ccb_waitp->mpii_ccbw_cv);
	mutex_exit(&mpii_ccb_waitp->mpii_ccbw_mtx);
}

static void
mpii_minphys(struct buf *bp)
{
	DNPRINTF(MPII_D_MISC, "mpii_minphys: %d\n", bp->b_bcount);

	/* XXX currently using MPII_MAXFER = MAXPHYS */
	if (bp->b_bcount > MPII_MAXFER) {
		bp->b_bcount = MPII_MAXFER;
		minphys(bp);
	}
}

static void
mpii_scsipi_request(struct scsipi_channel *chan, scsipi_adapter_req_t req,
    void *arg)
{
	struct scsipi_periph	*periph;
	struct scsipi_xfer	*xs;
	struct scsipi_adapter	*adapt = chan->chan_adapter;
	struct mpii_softc	*sc = device_private(adapt->adapt_dev);
	struct mpii_ccb		*ccb;
	struct mpii_ccb_bundle	*mcb;
	struct mpii_msg_scsi_io	*io;
	struct mpii_device	*dev;
	int			target;
	int timeout;

	DNPRINTF(MPII_D_CMD, "%s: mpii_scsipi_request\n", DEVNAME(sc));
	switch (req) {
	case ADAPTER_REQ_GROW_RESOURCES:
		/* Not supported. */
		return;
	case ADAPTER_REQ_SET_XFER_MODE:
	{
		struct scsipi_xfer_mode *xm = arg;
		xm->xm_mode = PERIPH_CAP_TQING;
		xm->xm_period = 0;
		xm->xm_offset = 0;
		scsipi_async_event(&sc->sc_chan, ASYNC_EVENT_XFER_MODE, xm);
		return;
	}
	case ADAPTER_REQ_RUN_XFER:
		break;
	}

	xs = arg;
	periph = xs->xs_periph;
	target = periph->periph_target;

	if (xs->cmdlen > MPII_CDB_LEN) {
		DNPRINTF(MPII_D_CMD, "%s: CBD too big %d\n",
		    DEVNAME(sc), xs->cmdlen);
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.scsi_sense.response_code =
		    SSD_RCODE_VALID | SSD_RCODE_CURRENT;
		xs->sense.scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.scsi_sense.asc = 0x20;
		xs->error = XS_SENSE;
		scsipi_done(xs);
		return;
	}

	if ((dev = sc->sc_devs[target]) == NULL) {
		/* device no longer exists */
		xs->error = XS_SELTIMEOUT;
		scsipi_done(xs);
		return;
	}

	ccb = mpii_get_ccb(sc, MPII_NOSLEEP);
	if (ccb == NULL) {
		xs->error = XS_RESOURCE_SHORTAGE;
		scsipi_done(xs);
		return;
	}

	DNPRINTF(MPII_D_CMD, "%s: ccb_smid: %d xs->xs_control: 0x%x\n",
	    DEVNAME(sc), ccb->ccb_smid, xs->xs_control);

	ccb->ccb_cookie = xs;
	ccb->ccb_done = mpii_scsi_cmd_done;
	ccb->ccb_dev_handle = dev->dev_handle;

	mcb = ccb->ccb_cmd;
	io = &mcb->mcb_io;

	io->function = MPII_FUNCTION_SCSI_IO_REQUEST;
	io->sense_buffer_length = sizeof(xs->sense);
	io->sgl_offset0 = 24; /* XXX fix this */
	io->io_flags = htole16(xs->cmdlen);
	io->dev_handle = htole16(ccb->ccb_dev_handle);
	io->lun[0] = htobe16(periph->periph_lun);

	switch (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
	case XS_CTL_DATA_IN:
		io->direction = MPII_SCSIIO_DIR_READ;
		break;
	case XS_CTL_DATA_OUT:
		io->direction = MPII_SCSIIO_DIR_WRITE;
		break;
	default:
		io->direction = MPII_SCSIIO_DIR_NONE;
	}

	io->tagging = MPII_SCSIIO_ATTR_SIMPLE_Q;

	memcpy(io->cdb, xs->cmd, xs->cmdlen);

	io->data_length = htole32(xs->datalen);

	io->sense_buffer_low_address = htole32(ccb->ccb_cmd_dva +
	    ((u_int8_t *)&mcb->mcb_sense - (u_int8_t *)mcb));

	if (mpii_load_xs(ccb) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		mpii_put_ccb(sc, ccb);
		scsipi_done(xs);
		return;
	}

	DNPRINTF(MPII_D_CMD, "%s:  sizeof(mpii_msg_scsi_io): %ld "
	    "sizeof(mpii_ccb_bundle): %ld sge offset: 0x%02lx\n",
	    DEVNAME(sc), sizeof(struct mpii_msg_scsi_io),
	    sizeof(struct mpii_ccb_bundle),
	    (u_int8_t *)&mcb->mcb_sgl[0] - (u_int8_t *)mcb);

	DNPRINTF(MPII_D_CMD, "%s   sgl[0]: 0x%04x 0%04x 0x%04x\n",
	    DEVNAME(sc), mcb->mcb_sgl[0].sg_hdr, mcb->mcb_sgl[0].sg_lo_addr,
	    mcb->mcb_sgl[0].sg_hi_addr);

	DNPRINTF(MPII_D_CMD, "%s:  Offset0: 0x%02x\n", DEVNAME(sc),
	    io->sgl_offset0);

	if (xs->xs_control & XS_CTL_POLL) {
		if (mpii_poll(sc, ccb) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
			mpii_put_ccb(sc, ccb);
			scsipi_done(xs);
		}
		return;
	}
	timeout = mstohz(xs->timeout);
	if (timeout == 0)
		timeout = 1;
	callout_reset(&xs->xs_callout, timeout, mpii_scsi_cmd_tmo, ccb);

	DNPRINTF(MPII_D_CMD, "%s:    mpii_scsipi_request(): opcode: %02x "
	    "datalen: %d\n", DEVNAME(sc), xs->cmd->opcode, xs->datalen);

	mpii_start(sc, ccb);
}

static void
mpii_scsi_cmd_tmo(void *xccb)
{
	struct mpii_ccb		*ccb = xccb;
	struct mpii_softc	*sc = ccb->ccb_sc;

	printf("%s: mpii_scsi_cmd_tmo\n", DEVNAME(sc));

	mutex_enter(&sc->sc_ccb_mtx);
	if (ccb->ccb_state == MPII_CCB_QUEUED) {
		ccb->ccb_state = MPII_CCB_TIMEOUT;
		workqueue_enqueue(sc->sc_ssb_tmowk, &ccb->u.ccb_wk, NULL);
	}
	mutex_exit(&sc->sc_ccb_mtx);
}

static void
mpii_scsi_cmd_tmo_handler(struct work *wk, void *cookie)
{
	struct mpii_softc			*sc = cookie;
	struct mpii_ccb				*tccb;
	struct mpii_ccb				*ccb;
	struct mpii_msg_scsi_task_request	*stq;

	ccb = (void *)wk;
	tccb = mpii_get_ccb(sc, 0);

	mutex_enter(&sc->sc_ccb_mtx);
	if (ccb->ccb_state != MPII_CCB_TIMEOUT) {
		mpii_put_ccb(sc, tccb);
	}
	/* should remove any other ccbs for the same dev handle */
	mutex_exit(&sc->sc_ccb_mtx);

	stq = tccb->ccb_cmd;
	stq->function = MPII_FUNCTION_SCSI_TASK_MGMT;
	stq->task_type = MPII_SCSI_TASK_TARGET_RESET;
	stq->dev_handle = htole16(ccb->ccb_dev_handle);

	tccb->ccb_done = mpii_scsi_cmd_tmo_done;
	mpii_start(sc, tccb);
}

static void
mpii_scsi_cmd_tmo_done(struct mpii_ccb *tccb)
{
        mpii_put_ccb(tccb->ccb_sc, tccb);
}

static u_int8_t
map_scsi_status(u_int8_t mpii_scsi_status)
{
	u_int8_t scsi_status;
	
	switch (mpii_scsi_status) 
	{
	case MPII_SCSIIO_ERR_STATUS_SUCCESS:
		scsi_status = SCSI_OK;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_CHECK_COND:
		scsi_status = SCSI_CHECK;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_BUSY:
		scsi_status = SCSI_BUSY;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_INTERMEDIATE:
		scsi_status = SCSI_INTERM;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_INTERMEDIATE_CONDMET:
		scsi_status = SCSI_INTERM;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_RESERVATION_CONFLICT:
		scsi_status = SCSI_RESV_CONFLICT;
		break;
		
	case MPII_SCSIIO_ERR_STATUS_CMD_TERM:
	case MPII_SCSIIO_ERR_STATUS_TASK_ABORTED:
		scsi_status = SCSI_TERMINATED;
		break;

	case MPII_SCSIIO_ERR_STATUS_TASK_SET_FULL:
		scsi_status = SCSI_QUEUE_FULL;
		break;

	case MPII_SCSIIO_ERR_STATUS_ACA_ACTIVE:
		scsi_status = SCSI_ACA_ACTIVE;
		break;

	default:
		/* XXX: for the lack of anything better and other than OK */
		scsi_status = 0xFF;
		break;
	}

	return scsi_status;
}

static void
mpii_scsi_cmd_done(struct mpii_ccb *ccb)
{
	struct mpii_msg_scsi_io_error	*sie;
	struct mpii_softc	*sc = ccb->ccb_sc;
	struct scsipi_xfer	*xs = ccb->ccb_cookie;
	struct mpii_ccb_bundle	*mcb = ccb->ccb_cmd;
	bus_dmamap_t		dmap = ccb->ccb_dmamap;
	bool			timeout = 0;

	callout_stop(&xs->xs_callout);
	mutex_enter(&sc->sc_ccb_mtx);
	if (ccb->ccb_state == MPII_CCB_TIMEOUT)
		timeout = 1;
	ccb->ccb_state = MPII_CCB_READY;
	mutex_exit(&sc->sc_ccb_mtx);

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, dmap);
	}

	xs->error = XS_NOERROR;
	xs->resid = 0;

	if (ccb->ccb_rcb == NULL) {
		/* no scsi error, we're ok so drop out early */
		xs->status = SCSI_OK;
		mpii_put_ccb(sc, ccb);
		scsipi_done(xs);
		return;
	}

	sie = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_CMD, "%s: mpii_scsi_cmd_done xs cmd: 0x%02x len: %d "
	    "xs_control 0x%x\n", DEVNAME(sc), xs->cmd->opcode, xs->datalen,
	    xs->xs_control);
	DNPRINTF(MPII_D_CMD, "%s:  dev_handle: %d msg_length: %d "
	    "function: 0x%02x\n", DEVNAME(sc), le16toh(sie->dev_handle),
	    sie->msg_length, sie->function);
	DNPRINTF(MPII_D_CMD, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    sie->vp_id, sie->vf_id);
	DNPRINTF(MPII_D_CMD, "%s:  scsi_status: 0x%02x scsi_state: 0x%02x "
	    "ioc_status: 0x%04x\n", DEVNAME(sc), sie->scsi_status,
	    sie->scsi_state, le16toh(sie->ioc_status));
	DNPRINTF(MPII_D_CMD, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    le32toh(sie->ioc_loginfo));
	DNPRINTF(MPII_D_CMD, "%s:  transfer_count: %d\n", DEVNAME(sc),
	    le32toh(sie->transfer_count));
	DNPRINTF(MPII_D_CMD, "%s:  sense_count: %d\n", DEVNAME(sc),
	    le32toh(sie->sense_count));
	DNPRINTF(MPII_D_CMD, "%s:  response_info: 0x%08x\n", DEVNAME(sc),
	    le32toh(sie->response_info));
	DNPRINTF(MPII_D_CMD, "%s:  task_tag: 0x%04x\n", DEVNAME(sc),
	    le16toh(sie->task_tag));
	DNPRINTF(MPII_D_CMD, "%s:  bidirectional_transfer_count: 0x%08x\n",
	    DEVNAME(sc), le32toh(sie->bidirectional_transfer_count));

	xs->status = map_scsi_status(sie->scsi_status);

	switch (le16toh(sie->ioc_status) & MPII_IOCSTATUS_MASK) {
	case MPII_IOCSTATUS_SCSI_DATA_UNDERRUN:
		switch (sie->scsi_status) {
		case MPII_SCSIIO_ERR_STATUS_CHECK_COND:
			xs->error = XS_SENSE;
			/*FALLTHROUGH*/
		case MPII_SCSIIO_ERR_STATUS_SUCCESS:
			xs->resid = xs->datalen - le32toh(sie->transfer_count);
			break;

		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case MPII_IOCSTATUS_SUCCESS:
	case MPII_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (sie->scsi_status) {
		case MPII_SCSIIO_ERR_STATUS_SUCCESS:
			/*
			 * xs->resid = 0; - already set above
			 *
			 * XXX: check whether UNDERUN strategy
			 * would be appropriate here too.
			 * that would allow joining these cases.
			 */
			break;

		case MPII_SCSIIO_ERR_STATUS_CHECK_COND:
			xs->error = XS_SENSE;
			break;
			
		case MPII_SCSIIO_ERR_STATUS_BUSY:
		case MPII_SCSIIO_ERR_STATUS_TASK_SET_FULL:
			xs->error = XS_BUSY;
			break;

		default:
			xs->error = XS_DRIVER_STUFFUP;
		}
		break;

	case MPII_IOCSTATUS_BUSY:
	case MPII_IOCSTATUS_INSUFFICIENT_RESOURCES:
		xs->error = XS_BUSY;
		break;

	case MPII_IOCSTATUS_SCSI_IOC_TERMINATED:
	case MPII_IOCSTATUS_SCSI_TASK_TERMINATED:
		xs->error = timeout ? XS_TIMEOUT : XS_RESET;
		break;

	case MPII_IOCSTATUS_SCSI_INVALID_DEVHANDLE:
	case MPII_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		xs->error = XS_SELTIMEOUT;
		break;

	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (sie->scsi_state & MPII_SCSIIO_ERR_STATE_AUTOSENSE_VALID)
		memcpy(&xs->sense, &mcb->mcb_sense, sizeof(xs->sense));

	DNPRINTF(MPII_D_CMD, "%s:  xs err: %d status: %#x\n", DEVNAME(sc),
	    xs->error, xs->status);

	mpii_push_reply(sc, ccb->ccb_rcb);
	mpii_put_ccb(sc, ccb);
	scsipi_done(xs);
}

#if 0
static int
mpii_ioctl_cache(struct scsi_link *link, u_long cmd, struct dk_cache *dc)
{
	struct mpii_softc *sc = (struct mpii_softc *)link->adapter_softc;
	struct mpii_device *dev = sc->sc_devs[link->target];
	struct mpii_cfg_raid_vol_pg0 *vpg;
	struct mpii_msg_raid_action_request *req;
 	struct mpii_msg_raid_action_reply *rep;
	struct mpii_cfg_hdr hdr;
	struct mpii_ccb	*ccb;
	u_int32_t addr = MPII_CFG_RAID_VOL_ADDR_HANDLE | dev->dev_handle;
	size_t pagelen;
	int rv = 0;
	int enabled;

	if (mpii_req_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0,
	    addr, MPII_PG_POLL, &hdr) != 0)
		return (EINVAL);

	pagelen = hdr.page_length * 4;
	vpg = malloc(pagelen, M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (vpg == NULL)
		return (ENOMEM);

	if (mpii_req_cfg_page(sc, addr, MPII_PG_POLL, &hdr, 1,
	    vpg, pagelen) != 0) {
		rv = EINVAL;
		goto done;
		free(vpg, M_TEMP);
		return (EINVAL);
	}

	enabled = ((le16toh(vpg->volume_settings) &
	    MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_MASK) ==
	    MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_ENABLED) ? 1 : 0;

	if (cmd == DIOCGCACHE) {
		dc->wrcache = enabled;
		dc->rdcache = 0;
		goto done;
	} /* else DIOCSCACHE */

	if (dc->rdcache) {
		rv = EOPNOTSUPP;
		goto done;
	}

	if (((dc->wrcache) ? 1 : 0) == enabled)
		goto done;

	ccb = mpii_get_ccb(sc, MPII_NOSLEEP);
	if (ccb == NULL) {
		rv = ENOMEM;
		goto done;
	}

	ccb->ccb_done = mpii_empty_done;

	req = ccb->ccb_cmd;
	bzero(req, sizeof(*req));
	req->function = MPII_FUNCTION_RAID_ACTION;
	req->action = MPII_RAID_ACTION_CHANGE_VOL_WRITE_CACHE;
	req->vol_dev_handle = htole16(dev->dev_handle);
	req->action_data = htole32(dc->wrcache ?
	    MPII_RAID_VOL_WRITE_CACHE_ENABLE :
	    MPII_RAID_VOL_WRITE_CACHE_DISABLE);

	if (mpii_poll(sc, ccb) != 0) {
		rv = EIO;
		goto done;
	}

	if (ccb->ccb_rcb != NULL) {
		rep = ccb->ccb_rcb->rcb_reply;
		if ((rep->ioc_status != MPII_IOCSTATUS_SUCCESS) ||
		    ((rep->action_data[0] &
		     MPII_RAID_VOL_WRITE_CACHE_MASK) !=
		    (dc->wrcache ? MPII_RAID_VOL_WRITE_CACHE_ENABLE :
		     MPII_RAID_VOL_WRITE_CACHE_DISABLE)))
			rv = EINVAL;
		mpii_push_reply(sc, ccb->ccb_rcb);
	}

	mpii_put_ccb(sc, ccb);

done:
	free(vpg, M_TEMP);
	return (rv);
}
#endif
static int
mpii_cache_enable(struct mpii_softc *sc, struct mpii_device *dev)
{
	struct mpii_cfg_raid_vol_pg0 *vpg;
	struct mpii_msg_raid_action_request *req;
 	struct mpii_msg_raid_action_reply *rep;
	struct mpii_cfg_hdr hdr;
	struct mpii_ccb	*ccb;
	u_int32_t addr = MPII_CFG_RAID_VOL_ADDR_HANDLE | dev->dev_handle;
	size_t pagelen;
	int rv = 0;
	int enabled;

	if (mpii_req_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0,
	    addr, MPII_PG_POLL, &hdr) != 0)
		return (EINVAL);

	pagelen = hdr.page_length * 4;
	vpg = malloc(pagelen, M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (vpg == NULL)
		return (ENOMEM);

	if (mpii_req_cfg_page(sc, addr, MPII_PG_POLL, &hdr, 1,
	    vpg, pagelen) != 0) {
		rv = EINVAL;
		goto done;
		free(vpg, M_TEMP);
		return (EINVAL);
	}

	enabled = ((le16toh(vpg->volume_settings) &
	    MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_MASK) ==
	    MPII_CFG_RAID_VOL_0_SETTINGS_CACHE_ENABLED) ? 1 : 0;
	aprint_normal_dev(sc->sc_dev, "target %d cache %s", dev->slot,
	    enabled ? "enabled" : "disabled, enabling");
	aprint_normal("\n");

	if (enabled == 0)
		goto done;

	ccb = mpii_get_ccb(sc, MPII_NOSLEEP);
	if (ccb == NULL) {
		rv = ENOMEM;
		goto done;
	}

	ccb->ccb_done = mpii_empty_done;

	req = ccb->ccb_cmd;
	bzero(req, sizeof(*req));
	req->function = MPII_FUNCTION_RAID_ACTION;
	req->action = MPII_RAID_ACTION_CHANGE_VOL_WRITE_CACHE;
	req->vol_dev_handle = htole16(dev->dev_handle);
	req->action_data = htole32(
	    MPII_RAID_VOL_WRITE_CACHE_ENABLE);

	if (mpii_poll(sc, ccb) != 0) {
		rv = EIO;
		goto done;
	}

	if (ccb->ccb_rcb != NULL) {
		rep = ccb->ccb_rcb->rcb_reply;
		if ((rep->ioc_status != MPII_IOCSTATUS_SUCCESS) ||
		    ((rep->action_data[0] &
		     MPII_RAID_VOL_WRITE_CACHE_MASK) !=
		     MPII_RAID_VOL_WRITE_CACHE_ENABLE))
			rv = EINVAL;
		mpii_push_reply(sc, ccb->ccb_rcb);
	}

	mpii_put_ccb(sc, ccb);

done:
	free(vpg, M_TEMP);
	if (rv) {
		aprint_error_dev(sc->sc_dev,
		    "enabling cache on target %d failed (%d)\n",
		    dev->slot, rv);
	}
	return (rv);
}

#if NBIO > 0
static int
mpii_ioctl(device_t dev, u_long cmd, void *addr)
{
	struct mpii_softc	*sc = device_private(dev);
	int			s, error = 0;

	DNPRINTF(MPII_D_IOCTL, "%s: mpii_ioctl ", DEVNAME(sc));
	KERNEL_LOCK(1, curlwp);
	s = splbio();

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(MPII_D_IOCTL, "inq\n");
		error = mpii_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;
	case BIOCVOL:
		DNPRINTF(MPII_D_IOCTL, "vol\n");
		error = mpii_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;
	case BIOCDISK:
		DNPRINTF(MPII_D_IOCTL, "disk\n");
		error = mpii_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;
	default:
		DNPRINTF(MPII_D_IOCTL, " invalid ioctl\n");
		error = EINVAL;
	}

	splx(s);
	KERNEL_UNLOCK_ONE(curlwp);
	return (error);
}

static int
mpii_ioctl_inq(struct mpii_softc *sc, struct bioc_inq *bi)
{
	int			i;

	DNPRINTF(MPII_D_IOCTL, "%s: mpii_ioctl_inq\n", DEVNAME(sc));

	strlcpy(bi->bi_dev, DEVNAME(sc), sizeof(bi->bi_dev));
	for (i = 0; i < sc->sc_max_devices; i++)
		if (sc->sc_devs[i] &&
		    ISSET(sc->sc_devs[i]->flags, MPII_DF_VOLUME))
			bi->bi_novol++;
	return (0);
}

static int
mpii_ioctl_vol(struct mpii_softc *sc, struct bioc_vol *bv)
{
	struct mpii_cfg_raid_vol_pg0	*vpg;
	struct mpii_cfg_hdr		hdr;
	struct mpii_device		*dev;
	struct scsipi_periph 		*periph;
	size_t				pagelen;
	u_int16_t			volh;
	int				rv, hcnt = 0;

	DNPRINTF(MPII_D_IOCTL, "%s: mpii_ioctl_vol %d\n",
	    DEVNAME(sc), bv->bv_volid);

	if ((dev = mpii_find_vol(sc, bv->bv_volid)) == NULL)
		return (ENODEV);
	volh = dev->dev_handle;

	if (mpii_req_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0,
	    MPII_CFG_RAID_VOL_ADDR_HANDLE | volh, 0, &hdr) != 0) {
		printf("%s: unable to fetch header for raid volume page 0\n",
		    DEVNAME(sc));
		return (EINVAL);
	}

	pagelen = hdr.page_length * 4;
	vpg = malloc(pagelen, M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (vpg == NULL) {
		printf("%s: unable to allocate space for raid "
		    "volume page 0\n", DEVNAME(sc));
		return (ENOMEM);
	}

	if (mpii_req_cfg_page(sc, MPII_CFG_RAID_VOL_ADDR_HANDLE | volh, 0,
	    &hdr, 1, vpg, pagelen) != 0) {
		printf("%s: unable to fetch raid volume page 0\n",
		    DEVNAME(sc));
		free(vpg, M_TEMP);
		return (EINVAL);
	}

	switch (vpg->volume_state) {
	case MPII_CFG_RAID_VOL_0_STATE_ONLINE:
	case MPII_CFG_RAID_VOL_0_STATE_OPTIMAL:
		bv->bv_status = BIOC_SVONLINE;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_DEGRADED:
		if (ISSET(le32toh(vpg->volume_status),
		    MPII_CFG_RAID_VOL_0_STATUS_RESYNC)) {
			bv->bv_status = BIOC_SVREBUILD;
			bv->bv_percent = dev->percent;
		} else
			bv->bv_status = BIOC_SVDEGRADED;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_FAILED:
		bv->bv_status = BIOC_SVOFFLINE;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_INITIALIZING:
		bv->bv_status = BIOC_SVBUILDING;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_MISSING:
	default:
		bv->bv_status = BIOC_SVINVALID;
		break;
	}

	switch (vpg->volume_type) {
	case MPII_CFG_RAID_VOL_0_TYPE_RAID0:
		bv->bv_level = 0;
		break;
	case MPII_CFG_RAID_VOL_0_TYPE_RAID1:
		bv->bv_level = 1;
		break;
	case MPII_CFG_RAID_VOL_0_TYPE_RAID1E:
	case MPII_CFG_RAID_VOL_0_TYPE_RAID10:
		bv->bv_level = 10;
		break;
	default:
		bv->bv_level = -1;
	}

	if ((rv = mpii_bio_hs(sc, NULL, 0, vpg->hot_spare_pool, &hcnt)) != 0) {
		free(vpg, M_TEMP);
		return (rv);
	}

	bv->bv_nodisk = vpg->num_phys_disks + hcnt;

	bv->bv_size = le64toh(vpg->max_lba) * le16toh(vpg->block_size);

	periph = scsipi_lookup_periph(&sc->sc_chan, dev->slot, 0);
	if (periph != NULL) {
		if (periph->periph_dev == NULL) {
			snprintf(bv->bv_dev, sizeof(bv->bv_dev), "%s:%d",
			    DEVNAME(sc), dev->slot);
		} else {
			strlcpy(bv->bv_dev, device_xname(periph->periph_dev),
			    sizeof(bv->bv_dev));
		}
	}

	free(vpg, M_TEMP);
	return (0);
}

static int
mpii_ioctl_disk(struct mpii_softc *sc, struct bioc_disk *bd)
{
	struct mpii_cfg_raid_vol_pg0		*vpg;
	struct mpii_cfg_raid_vol_pg0_physdisk	*pd;
	struct mpii_cfg_hdr			hdr;
	struct mpii_device			*dev;
	size_t					pagelen;
	u_int16_t				volh;
	u_int8_t				dn;

	DNPRINTF(MPII_D_IOCTL, "%s: mpii_ioctl_disk %d/%d\n",
	    DEVNAME(sc), bd->bd_volid, bd->bd_diskid);

	if ((dev = mpii_find_vol(sc, bd->bd_volid)) == NULL)
		return (ENODEV);
	volh = dev->dev_handle;

	if (mpii_req_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0,
	    MPII_CFG_RAID_VOL_ADDR_HANDLE | volh, 0, &hdr) != 0) {
		printf("%s: unable to fetch header for raid volume page 0\n",
		    DEVNAME(sc));
		return (EINVAL);
	}

	pagelen = hdr.page_length * 4;
	vpg = malloc(pagelen, M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (vpg == NULL) {
		printf("%s: unable to allocate space for raid "
		    "volume page 0\n", DEVNAME(sc));
		return (ENOMEM);
	}

	if (mpii_req_cfg_page(sc, MPII_CFG_RAID_VOL_ADDR_HANDLE | volh, 0,
	    &hdr, 1, vpg, pagelen) != 0) {
		printf("%s: unable to fetch raid volume page 0\n",
		    DEVNAME(sc));
		free(vpg, M_TEMP);
		return (EINVAL);
	}

	if (bd->bd_diskid >= vpg->num_phys_disks) {
		int		nvdsk = vpg->num_phys_disks;
		int		hsmap = vpg->hot_spare_pool;

		free(vpg, M_TEMP);
		return (mpii_bio_hs(sc, bd, nvdsk, hsmap, NULL));
	}

	pd = (struct mpii_cfg_raid_vol_pg0_physdisk *)(vpg + 1) +
	    bd->bd_diskid;
	dn = pd->phys_disk_num;

	free(vpg, M_TEMP);
	return (mpii_bio_disk(sc, bd, dn));
}

static int
mpii_bio_hs(struct mpii_softc *sc, struct bioc_disk *bd, int nvdsk,
     int hsmap, int *hscnt)
{
	struct mpii_cfg_raid_config_pg0	*cpg;
	struct mpii_raid_config_element	*el;
	struct mpii_ecfg_hdr		ehdr;
	size_t				pagelen;
	int				i, nhs = 0;

	if (bd) {
		DNPRINTF(MPII_D_IOCTL, "%s: mpii_bio_hs %d\n", DEVNAME(sc),
		    bd->bd_diskid - nvdsk);
	} else {
		DNPRINTF(MPII_D_IOCTL, "%s: mpii_bio_hs\n", DEVNAME(sc));
	}

	if (mpii_req_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_CONFIG,
	    0, MPII_CFG_RAID_CONFIG_ACTIVE_CONFIG, MPII_PG_EXTENDED,
	    &ehdr) != 0) {
		printf("%s: unable to fetch header for raid config page 0\n",
		    DEVNAME(sc));
		return (EINVAL);
	}

	pagelen = le16toh(ehdr.ext_page_length) * 4;
	cpg = malloc(pagelen, M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (cpg == NULL) {
		printf("%s: unable to allocate space for raid config page 0\n",
		    DEVNAME(sc));
		return (ENOMEM);
	}

	if (mpii_req_cfg_page(sc, MPII_CFG_RAID_CONFIG_ACTIVE_CONFIG,
	    MPII_PG_EXTENDED, &ehdr, 1, cpg, pagelen) != 0) {
		printf("%s: unable to fetch raid config page 0\n",
		    DEVNAME(sc));
		free(cpg, M_TEMP);
		return (EINVAL);
	}

	el = (struct mpii_raid_config_element *)(cpg + 1);
	for (i = 0; i < cpg->num_elements; i++, el++) {
		if (ISSET(le16toh(el->element_flags),
		    MPII_RAID_CONFIG_ELEMENT_FLAG_HSP_PHYS_DISK) &&
		    el->hot_spare_pool == hsmap) {
			/*
			 * diskid comparison is based on the idea that all
			 * disks are counted by the bio(4) in sequence, thus
			 * substracting the number of disks in the volume
			 * from the diskid yields us a "relative" hotspare
			 * number, which is good enough for us.
			 */
			if (bd != NULL && bd->bd_diskid == nhs + nvdsk) {
				u_int8_t dn = el->phys_disk_num;

				free(cpg, M_TEMP);
				return (mpii_bio_disk(sc, bd, dn));
			}
			nhs++;
		}
	}

	if (hscnt)
		*hscnt = nhs;

	free(cpg, M_TEMP);
	return (0);
}

static int
mpii_bio_disk(struct mpii_softc *sc, struct bioc_disk *bd, u_int8_t dn)
{
	struct mpii_cfg_raid_physdisk_pg0	*ppg;
	struct mpii_cfg_hdr			hdr;
	struct mpii_device			*dev;
	int					len;

	DNPRINTF(MPII_D_IOCTL, "%s: mpii_bio_disk %d\n", DEVNAME(sc),
	    bd->bd_diskid);

	ppg = malloc(sizeof(*ppg), M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
	if (ppg == NULL) {
		printf("%s: unable to allocate space for raid physical disk "
		    "page 0\n", DEVNAME(sc));
		return (ENOMEM);
	}

	hdr.page_version = 0;
	hdr.page_length = sizeof(*ppg) / 4;
	hdr.page_number = 0;
	hdr.page_type = MPII_CONFIG_REQ_PAGE_TYPE_RAID_PD;

	if (mpii_req_cfg_page(sc, MPII_CFG_RAID_PHYS_DISK_ADDR_NUMBER | dn, 0,
	    &hdr, 1, ppg, sizeof(*ppg)) != 0) {
		printf("%s: unable to fetch raid drive page 0\n",
		    DEVNAME(sc));
		free(ppg, M_TEMP);
		return (EINVAL);
	}

	bd->bd_target = ppg->phys_disk_num;

	if ((dev = mpii_find_dev(sc, le16toh(ppg->dev_handle))) == NULL) {
		bd->bd_status = BIOC_SDINVALID;
		free(ppg, M_TEMP);
		return (0);
	}

	switch (ppg->phys_disk_state) {
	case MPII_CFG_RAID_PHYDISK_0_STATE_ONLINE:
	case MPII_CFG_RAID_PHYDISK_0_STATE_OPTIMAL:
		bd->bd_status = BIOC_SDONLINE;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_OFFLINE:
		if (ppg->offline_reason ==
		    MPII_CFG_RAID_PHYDISK_0_OFFLINE_FAILED ||
		    ppg->offline_reason ==
		    MPII_CFG_RAID_PHYDISK_0_OFFLINE_FAILEDREQ)
			bd->bd_status = BIOC_SDFAILED;
		else
			bd->bd_status = BIOC_SDOFFLINE;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_DEGRADED:
		bd->bd_status = BIOC_SDFAILED;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_REBUILDING:
		bd->bd_status = BIOC_SDREBUILD;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_HOTSPARE:
		bd->bd_status = BIOC_SDHOTSPARE;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_NOTCONFIGURED:
		bd->bd_status = BIOC_SDUNUSED;
		break;
	case MPII_CFG_RAID_PHYDISK_0_STATE_NOTCOMPATIBLE:
	default:
		bd->bd_status = BIOC_SDINVALID;
		break;
	}

	bd->bd_size = le64toh(ppg->dev_max_lba) * le16toh(ppg->block_size);

	scsipi_strvis(bd->bd_vendor, sizeof(bd->bd_vendor),
	    ppg->vendor_id, sizeof(ppg->vendor_id));
	len = strlen(bd->bd_vendor);
	bd->bd_vendor[len] = ' ';
	scsipi_strvis(&bd->bd_vendor[len + 1], sizeof(ppg->vendor_id) - len - 1,
	    ppg->product_id, sizeof(ppg->product_id));
	scsipi_strvis(bd->bd_serial, sizeof(bd->bd_serial),
	    ppg->serial, sizeof(ppg->serial));

	free(ppg, M_TEMP);
	return (0);
}

static struct mpii_device *
mpii_find_vol(struct mpii_softc *sc, int volid)
{
	struct mpii_device	*dev = NULL;

	if (sc->sc_vd_id_low + volid >= sc->sc_max_devices)
		return (NULL);
	dev = sc->sc_devs[sc->sc_vd_id_low + volid];
	if (dev && ISSET(dev->flags, MPII_DF_VOLUME))
		return (dev);
	return (NULL);
}

/*
 * Non-sleeping lightweight version of the mpii_ioctl_vol
 */
static int
mpii_bio_volstate(struct mpii_softc *sc, struct bioc_vol *bv)
{
	struct mpii_cfg_raid_vol_pg0	*vpg;
	struct mpii_cfg_hdr		hdr;
	struct mpii_device		*dev = NULL;
	size_t				pagelen;
	u_int16_t			volh;

	if ((dev = mpii_find_vol(sc, bv->bv_volid)) == NULL)
		return (ENODEV);
	volh = dev->dev_handle;

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0,
	    MPII_CFG_RAID_VOL_ADDR_HANDLE | volh, &hdr) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch header for raid "
		    "volume page 0\n", DEVNAME(sc));
		return (EINVAL);
	}

	pagelen = hdr.page_length * 4;
	vpg = malloc(pagelen, M_TEMP, M_NOWAIT | M_ZERO);
	if (vpg == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: unable to allocate space for raid "
		    "volume page 0\n", DEVNAME(sc));
		return (ENOMEM);
	}

	if (mpii_cfg_page(sc, MPII_CFG_RAID_VOL_ADDR_HANDLE | volh,
	    &hdr, 1, vpg, pagelen) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch raid volume "
		    "page 0\n", DEVNAME(sc));
		free(vpg, M_TEMP);
		return (EINVAL);
	}

	switch (vpg->volume_state) {
	case MPII_CFG_RAID_VOL_0_STATE_ONLINE:
	case MPII_CFG_RAID_VOL_0_STATE_OPTIMAL:
		bv->bv_status = BIOC_SVONLINE;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_DEGRADED:
		if (ISSET(le32toh(vpg->volume_status),
		    MPII_CFG_RAID_VOL_0_STATUS_RESYNC))
			bv->bv_status = BIOC_SVREBUILD;
		else
			bv->bv_status = BIOC_SVDEGRADED;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_FAILED:
		bv->bv_status = BIOC_SVOFFLINE;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_INITIALIZING:
		bv->bv_status = BIOC_SVBUILDING;
		break;
	case MPII_CFG_RAID_VOL_0_STATE_MISSING:
	default:
		bv->bv_status = BIOC_SVINVALID;
		break;
	}

	free(vpg, M_TEMP);
	return (0);
}

static int
mpii_create_sensors(struct mpii_softc *sc)
{
	int			i, rv;

	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sensors = malloc(sizeof(envsys_data_t) * sc->sc_vd_count,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_sensors == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "can't allocate envsys_data_t\n");
		return (1);
	}

	for (i = 0; i < sc->sc_vd_count; i++) {
		sc->sc_sensors[i].units = ENVSYS_DRIVE;
		sc->sc_sensors[i].state = ENVSYS_SINVALID;
		sc->sc_sensors[i].value_cur = ENVSYS_DRIVE_EMPTY;
		/* Enable monitoring for drive state changes */
		sc->sc_sensors[i].flags |= ENVSYS_FMONSTCHANGED;

		/* logical drives */
		snprintf(sc->sc_sensors[i].desc,
		    sizeof(sc->sc_sensors[i].desc), "%s:%d",
		    DEVNAME(sc), i);
		if ((rv = sysmon_envsys_sensor_attach(sc->sc_sme,
		    &sc->sc_sensors[i])) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "unable to attach sensor (rv = %d)\n", rv);
			goto out;
		}
	}
	sc->sc_sme->sme_name =  DEVNAME(sc);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = mpii_refresh_sensors;

	rv = sysmon_envsys_register(sc->sc_sme);

	if (rv != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to register with sysmon (rv = %d)\n", rv);
		goto out;
	}
	return 0;

out:
	free(sc->sc_sensors, M_DEVBUF);
	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
	return EINVAL;
}

static int
mpii_destroy_sensors(struct mpii_softc *sc)
{
	if (sc->sc_sme == NULL)
		return 0;
	sysmon_envsys_unregister(sc->sc_sme);
	sc->sc_sme = NULL;
	free(sc->sc_sensors, M_DEVBUF);
	return 0;
}

static void
mpii_refresh_sensors(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct mpii_softc	*sc = sc = sme->sme_cookie;
	struct bioc_vol		bv;
	int			s, error;

	bzero(&bv, sizeof(bv));
	bv.bv_volid = edata->sensor;
	KERNEL_LOCK(1, curlwp);
	s = splbio();
	error = mpii_bio_volstate(sc, &bv);
	splx(s);
	KERNEL_UNLOCK_ONE(curlwp);
	if (error)
		bv.bv_status = BIOC_SVINVALID;
	bio_vol_to_envsys(edata, &bv);
}
#endif /* NBIO > 0 */
