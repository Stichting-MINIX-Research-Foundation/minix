/*	$NetBSD: twareg.h,v 1.11 2010/11/22 23:02:16 dholland Exp $ */
/*	$wasabi: twareg.h,v 1.14 2006/07/28 18:29:51 wrstuden Exp $ */

/*-
 * Copyright (c) 2003-04 3ware, Inc.
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
 *	$FreeBSD: src/sys/dev/twa/twa_reg.h,v 1.2 2004/08/18 16:14:44 vkashyap Exp $
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */

#ifndef _PCI_TWAREG_H_
#define	_PCI_TWAREG_H_

#if defined(_KERNEL)
#include <sys/bus.h>

/*
 * The following macro has no business being in twa_reg.h.  It should probably
 * be defined in twa_includes.h, before the #include twa_reg.h....  But that
 * causes the API to run into build errors.  Will leave it here for now...
 */
#define TWA_64BIT_ADDRESSES	((sizeof(bus_addr_t) == 8) ? 1 : 0)

/*
 * Define the following here since it relies on TWA_64BIT_ADDRESSES which
 * depends on sizeof(bus_addr_t), which is not exported to userland.
 * The userland API shouldn't care about the kernel's bus_addr_t.
 * For the userland API, use the array size that we would use for 32-bit
 * addresses since that's what we use in the sg structure definition.
 * The userland API does not actually appear to use the array, but it
 * does include the array in various command structures.
 */
#define TWA_MAX_SG_ELEMENTS		(TWA_64BIT_ADDRESSES ? 70 : 105)
#else
#define TWA_MAX_SG_ELEMENTS		105
#endif

#define TWAQ_FREE	0
#define TWAQ_BUSY	1
#define TWAQ_PENDING	2
#define TWAQ_COMPLETE	3
#define TWAQ_IO_PENDING 4
#define TWAQ_COUNT	5	/* total number of queues */

#define TWA_DRIVER_VERSION_STRING		"1.00.00.000"

#define TWA_REQUEST_TIMEOUT_PERIOD		60 /* seconds */

#define TWA_MESSAGE_SOURCE_CONTROLLER_ERROR	3

/* Register offsets from base address. */
#define	TWA_CONTROL_REGISTER_OFFSET		0x0
#define	TWA_STATUS_REGISTER_OFFSET		0x4
#define	TWA_COMMAND_QUEUE_OFFSET		0x8
#define	TWA_RESPONSE_QUEUE_OFFSET		0xC
#define	TWA_COMMAND_QUEUE_OFFSET_LOW		0x20
#define	TWA_COMMAND_QUEUE_OFFSET_HIGH		0x24
#define	TWA_RESPONSE_QUEUE_LARGE_OFFSET		0x30

#if defined(_KERNEL)
#define TWA_WRITE_REGISTER(sc, offset, val)	\
	bus_space_write_4(sc->twa_bus_iot, sc->twa_bus_ioh, offset, (uint32_t)val)

#define TWA_WRITE_COMMAND_QUEUE(sc, val)				\
	do {								\
		if (TWA_64BIT_ADDRESSES) {				\
			/* First write the low 4 bytes, then the high 4. */  \
			TWA_WRITE_REGISTER(sc, TWA_COMMAND_QUEUE_OFFSET_LOW, \
						(uint32_t)(val));	\
			TWA_WRITE_REGISTER(sc, TWA_COMMAND_QUEUE_OFFSET_HIGH,\
					(uint32_t)(((uint64_t)val)>>32));  \
		} else							\
			TWA_WRITE_REGISTER(sc, TWA_COMMAND_QUEUE_OFFSET,\
						(uint32_t)(val)); \
	} while (0)
#endif

#define TWA_WRITE_COMMAND_QUEUE_HIGH(sc, val)				\
	do {								\
		TWA_WRITE_REGISTER(sc, TWA_COMMAND_QUEUE_OFFSET_HIGH,	\
				(uint32_t)(((uint64_t)val)>>32));	\
	} while (0)

#define TWA_WRITE_COMMAND_QUEUE_LOW(sc, val)				\
	do {								\
		TWA_WRITE_REGISTER(sc, TWA_COMMAND_QUEUE_OFFSET_LOW,	\
				(uint32_t)(val));			\
	} while (0)

/* Control register bit definitions. */
#define TWA_CONTROL_ISSUE_HOST_INTERRUPT	0x00000020
#define TWA_CONTROL_DISABLE_INTERRUPTS		0x00000040
#define TWA_CONTROL_ENABLE_INTERRUPTS		0x00000080
#define TWA_CONTROL_ISSUE_SOFT_RESET		0x00000100
#define TWA_CONTROL_CLEAR_ERROR_STATUS		0x00000200
#define TWA_CONTROL_UNMASK_RESPONSE_INTERRUPT	0x00004000
#define TWA_CONTROL_UNMASK_COMMAND_INTERRUPT	0x00008000
#define TWA_CONTROL_MASK_RESPONSE_INTERRUPT	0x00010000
#define TWA_CONTROL_MASK_COMMAND_INTERRUPT	0x00020000
#define TWA_CONTROL_CLEAR_ATTENTION_INTERRUPT	0x00040000
#define TWA_CONTROL_CLEAR_HOST_INTERRUPT	0x00080000
#define TWA_CONTROL_CLEAR_PCI_ABORT		0x00100000
#define TWA_CONTROL_CLEAR_QUEUE_ERROR		0x00400000
#define TWA_CONTROL_CLEAR_PARITY_ERROR		0x00800000

/* Status register bit definitions. */
#define TWA_STATUS_ROM_BIOS_IN_SBUF		0x00000002
#define TWA_STATUS_COMMAND_QUEUE_EMPTY		0x00001000
#define TWA_STATUS_MICROCONTROLLER_READY	0x00002000
#define TWA_STATUS_RESPONSE_QUEUE_EMPTY		0x00004000
#define TWA_STATUS_COMMAND_QUEUE_FULL		0x00008000
#define TWA_STATUS_RESPONSE_INTERRUPT		0x00010000
#define TWA_STATUS_COMMAND_INTERRUPT		0x00020000
#define TWA_STATUS_ATTENTION_INTERRUPT		0x00040000
#define TWA_STATUS_HOST_INTERRUPT		0x00080000
#define TWA_STATUS_PCI_ABORT_INTERRUPT		0x00100000
#define TWA_STATUS_MICROCONTROLLER_ERROR	0x00200000
#define TWA_STATUS_QUEUE_ERROR_INTERRUPT	0x00400000
#define TWA_STATUS_PCI_PARITY_ERROR_INTERRUPT	0x00800000
#define TWA_STATUS_MINOR_VERSION_MASK		0x0F000000
#define TWA_STATUS_MAJOR_VERSION_MASK		0xF0000000

#define TWA_STATUS_EXPECTED_BITS		0x00002000
#define TWA_STATUS_UNEXPECTED_BITS		0x00F00000

/* For use with the %b printf format. */
#define TWA_STATUS_BITS_DESCRIPTION \
	"\20\15CMD_Q_EMPTY\16MC_RDY\17RESP_Q_EMPTY\20CMD_Q_FULL\21RESP_INTR\22CMD_INTR\23ATTN_INTR\24HOST_INTR\25PCI_ABRT\26MC_ERR\27Q_ERR\30PCI_PERR\n"

/* Detect inconsistencies in the status register. */
#define TWA_STATUS_ERRORS(x)			\
	((x & TWA_STATUS_UNEXPECTED_BITS) &&	\
	 (x & TWA_STATUS_MICROCONTROLLER_READY))

/* PCI related defines. */
#define TWA_IO_CONFIG_REG		0x10
#define TWA_DEVICE_NAME			"3ware 9000 series Storage Controller"
#define TWA_VENDOR_ID			0x13C1
#define TWA_DEVICE_ID_9K		0x1002

#define TWA_PCI_CONFIG_CLEAR_PARITY_ERROR	0xc100
#define TWA_PCI_CONFIG_CLEAR_PCI_ABORT		0x2000
#define TWA_9550SX_DRAIN_COMPLETE		0xffff

/* Command packet opcodes. */
#define TWA_OP_NOP			0x00
#define TWA_OP_INIT_CONNECTION		0x01
#define TWA_OP_READ			0x02
#define TWA_OP_WRITE			0x03
#define TWA_OP_READVERIFY		0x04
#define TWA_OP_VERIFY			0x05
#define TWA_OP_ZEROUNIT			0x08
#define TWA_OP_REPLACEUNIT		0x09
#define TWA_OP_HOTSWAP			0x0A
#define TWA_OP_SELFTESTS		0x0B
#define TWA_OP_SYNC_PARAM		0x0C
#define TWA_OP_REORDER_UNITS		0x0D
#define TWA_OP_FLUSH			0x0E
#define TWA_OP_EXECUTE_SCSI_COMMAND	0x10
#define TWA_OP_ATA_PASSTHROUGH		0x11
#define TWA_OP_GET_PARAM		0x12
#define TWA_OP_SET_PARAM		0x13
#define TWA_OP_CREATEUNIT		0x14
#define TWA_OP_DELETEUNIT		0x15
#define TWA_OP_DOWNLOAD_FIRMWARE	0x16
#define TWA_OP_REBUILDUNIT		0x17
#define TWA_OP_POWER_MANAGEMENT		0x18

#define TWA_OP_REMOTE_PRINT		0x1B
#define TWA_OP_RESET_FIRMWARE		0x1C
#define TWA_OP_DEBUG			0x1D

#define TWA_OP_DIAGNOSTICS		0x1F

/* Misc defines. */
#define TWA_ALIGNMENT			0x4
#define TWA_MAX_UNITS			16
#define TWA_9650_MAX_UNITS		32
#define TWA_9690_MAX_UNITS		32
#define TWA_INIT_MESSAGE_CREDITS	0x100
#define TWA_SHUTDOWN_MESSAGE_CREDITS	0x001
#define TWA_64BIT_SG_ADDRESSES		0x00000001
#define TWA_EXTENDED_INIT_CONNECT	0x00000002
#define TWA_BASE_MODE			1
#define TWA_BASE_FW_SRL			24
#define TWA_BASE_FW_BRANCH		0
#define TWA_BASE_FW_BUILD		1
#define TWA_CURRENT_FW_SRL		28
#define TWA_CURRENT_FW_BRANCH		4
#define TWA_CURRENT_FW_BUILD		9
#define TWA_9000_ARCH_ID		0x5	/* 9000 series controllers */
#define TWA_CTLR_FW_SAME_OR_NEWER	0x00000001
#define TWA_CTLR_FW_COMPATIBLE		0x00000002
#define TWA_BUNDLED_FW_SAFE_TO_FLASH	0x00000004
#define TWA_CTLR_FW_RECOMMENDS_FLASH	0x00000008
#define NUM_FW_IMAGE_CHUNKS		5
#define TWA_MAX_IO_SIZE			0x20000	/* 128K */
/* #define TWA_MAX_SG_ELEMENTS		defined above */
#define TWA_MAX_ATA_SG_ELEMENTS		60
#define TWA_Q_LENGTH			TWA_INIT_MESSAGE_CREDITS
#define TWA_MAX_RESET_TRIES		3
#define TWA_SECTOR_SIZE			0x200	/* generic I/O bufffer */
#define TWA_SENSE_DATA_LENGTH		18
#define TWA_MICROSECOND                 1000000
#define TWA_ERROR_LOGICAL_UNIT_NOT_SUPPORTED	0x010a
#define TWA_ERROR_UNIT_OFFLINE			0x0128
#define TWA_ERROR_MORE_DATA			0x0231

/* Scatter/Gather list entry. */
struct twa_sg {
#if defined(_KERNEL)
	bus_addr_t	address;
#else
	uint32_t	xx_address_xx;	/* Fail if userland tries to use this */
#endif
	uint32_t	length;
} __packed;


/* 7000 structures. */
struct twa_command_init_connect {
	uint8_t		opcode:5;	/* TWA_OP_INITCONNECTION */
	uint8_t		res1:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		res2;
	uint8_t		status;
	uint8_t		flags;
	uint16_t	message_credits;
	uint32_t	features;
	uint16_t	fw_srl;
	uint16_t	fw_arch_id;
	uint16_t	fw_branch;
	uint16_t	fw_build;
	uint32_t	result;
}__packed;

struct twa_command_download_firmware {
	uint8_t		opcode:5;	/* TWA_DOWNLOAD_FIRMWARE */
	uint8_t		sgl_offset:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit;
	uint8_t		status;
	uint8_t		flags;
	uint16_t	param;
	uint8_t		sgl[1];
} __packed;


struct twa_command_reset_firmware {
	uint8_t		opcode:5;	/* TWA_OP_RESET_FIRMWARE */
	uint8_t		res1:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit;
	uint8_t		status;
	uint8_t		flags;
	uint8_t		res2;
	uint8_t		param;
} __packed;


struct twa_command_io {
	uint8_t		opcode:5;	/* TWA_OP_READ/TWA_OP_WRITE */
	uint8_t		sgl_offset:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
	uint16_t	block_count;
	uint32_t	lba;
	struct twa_sg	sgl[TWA_MAX_SG_ELEMENTS];
} __packed;


struct twa_command_hotswap {
	uint8_t		opcode:5;	/* TWA_OP_HOTSWAP */
	uint8_t		res1:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
	uint8_t		action;
#define TWA_OP_HOTSWAP_REMOVE		0x00	/* remove assumed-degraded unit */
#define TWA_OP_HOTSWAP_ADD_CBOD		0x01	/* add CBOD to empty port */
#define TWA_OP_HOTSWAP_ADD_SPARE	0x02	/* add spare to empty port */
	uint8_t		aport;
} __packed;


struct twa_command_param {
	uint8_t		opcode:5;	/* TWA_OP_GETPARAM, TWA_OP_SETPARAM */
	uint8_t		sgl_offset:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
	uint16_t	param_count;
	uint8_t		sgl[1];
} __packed;


struct twa_command_rebuildunit {
	uint8_t		opcode:5;	/* TWA_OP_REBUILDUNIT */
	uint8_t		res1:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		src_unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
	uint8_t		action:7;
#define TWA_OP_REBUILDUNIT_NOP		0
#define TWA_OP_REBUILDUNIT_STOP		2	/* stop all rebuilds */
#define TWA_OP_REBUILDUNIT_START	4	/* start rebuild with lowest unit */
#define TWA_OP_REBUILDUNIT_STARTUNIT	5	/* rebuild src_unit (not supported) */
	uint8_t		cs:1;			/* request state change on src_unit */
	uint8_t		logical_subunit;	/* for RAID10 rebuild of logical subunit */
} __packed;


struct twa_command_ata {
	uint8_t		opcode:5;	/* TWA_OP_ATA_PASSTHROUGH */
	uint8_t		sgl_offset:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
	uint16_t	param;
	uint16_t	features;
	uint16_t	sector_count;
	uint16_t	sector_num;
	uint16_t	cylinder_lo;
	uint16_t	cylinder_hi;
	uint8_t		drive_head;
	uint8_t		command;
	struct twa_sg	sgl[TWA_MAX_ATA_SG_ELEMENTS];
} __packed;


struct twa_command_generic {
	uint8_t		opcode:5;
	uint8_t		sgl_offset:3;
	uint8_t		size;
	uint8_t		request_id;
	uint8_t		unit:4;
	uint8_t		host_id:4;
	uint8_t		status;
	uint8_t		flags;
#define TWA_FLAGS_SUCCESS	0x00
#define TWA_FLAGS_INFORMATIONAL	0x01
#define TWA_FLAGS_WARNING	0x02
#define TWA_FLAGS_FATAL		0x03
#define TWA_FLAGS_PERCENTAGE	(1<<8)	/* bits 0-6 indicate completion percentage */
	uint16_t	count;		/* block count, parameter count, message credits */
} __packed;

/* Command packet header. */
#pragma pack(1)
struct twa_command_header {
	uint8_t		sense_data[TWA_SENSE_DATA_LENGTH];
	struct {
		int8_t		reserved[4];
		uint16_t	error;
		uint8_t		padding;
		struct {
			uint8_t		severity:3;
			uint8_t		reserved:5;
		} substatus_block;
	} status_block;
	uint8_t		err_specific_desc[98];
	struct {
		uint8_t		size_header;
		uint16_t	reserved;
		uint8_t		size_sense;
	} header_desc;
} __packed;
#pragma pack()


/* Command packet - must be TWA_ALIGNMENT aligned. */
union twa_command_7k {
	struct twa_command_init_connect		init_connect;
	struct twa_command_download_firmware	download_fw;
	struct twa_command_reset_firmware	reset_fw;
	struct twa_command_param		param;
	struct twa_command_generic		generic;
	uint8_t padding[1024 - sizeof(struct twa_command_header)];
} __packed;


/* 9000 structures. */

/* Command Packet. */
struct twa_command_9k {
	struct {
		uint8_t		opcode:5;
		uint8_t		reserved:3;
	} command;
	uint8_t		unit;
	uint16_t	request_id;
	uint8_t		status;
	uint8_t		sgl_offset; /* offset (in bytes) to sg_list, from the end of sgl_entries */
	uint16_t	sgl_entries;
	uint8_t		cdb[16];
	struct twa_sg	sg_list[TWA_MAX_SG_ELEMENTS];
	uint8_t		padding[32];
} __packed;



/* Full command packet. */
struct twa_command_packet {
	struct twa_command_header	cmd_hdr;
	union {
		union twa_command_7k	cmd_pkt_7k;
		struct twa_command_9k 	cmd_pkt_9k;
	} command;
} __packed;


/* Response queue entry. */
union twa_response_queue {
	struct {
		uint32_t	undefined_1:4;
		uint32_t	response_id:8;
		uint32_t	undefined_2:20;
	} u;
	uint32_t	value;
} __packed;


#define TWA_AEN_QUEUE_EMPTY		0x00
#define TWA_AEN_SOFT_RESET		0x01
#define TWA_AEN_SYNC_TIME_WITH_HOST	0x31
#define TWA_AEN_SEVERITY_ERROR		0x1
#define TWA_AEN_SEVERITY_WARNING	0x2
#define TWA_AEN_SEVERITY_INFO		0x3
#define TWA_AEN_SEVERITY_DEBUG		0x4

#define	TWA_PARAM_DRIVESUMMARY		0x0002
#define TWA_PARAM_DRIVESTATUS		3

#define TWA_DRIVE_DETECTED		0x80

#define	TWA_PARAM_DRIVE_TABLE		0x0200
#define TWA_PARAM_DRIVESIZEINDEX	2
#define TWA_PARAM_DRIVEMODELINDEX	3

#define TWA_PARAM_DRIVESIZE_LENGTH	4
#define TWA_PARAM_DRIVEMODEL_LENGTH	40


#define TWA_PARAM_VERSION		0x0402
#define TWA_PARAM_VERSION_Mon		2	/* monitor version [16] */
#define TWA_PARAM_VERSION_FW		3	/* firmware version [16] */
#define TWA_PARAM_VERSION_BIOS		4	/* BIOSs version [16] */
#define TWA_PARAM_VERSION_PCBA		5	/* PCB version [8] */
#define TWA_PARAM_VERSION_ATA		6	/* A-chip version [8] */
#define TWA_PARAM_VERSION_PCI		7	/* P-chip version [8] */

#define TWA_PARAM_CONTROLLER		0x0403
#define TWA_PARAM_CONTROLLER_PortCount	3	/* number of ports [1] */

#define TWA_PARAM_TIME_TABLE		0x40A
#define TWA_PARAM_TIME_SchedulerTime	0x3

#define TWA_9K_PARAM_DESCRIPTOR		0x8000


struct twa_param_9k {
	uint16_t	table_id;
	uint8_t		parameter_id;
	uint8_t		reserved;
	uint16_t	parameter_size_bytes;
	uint16_t	parameter_actual_size_bytes;
	uint8_t		data[1];
} __packed;

#endif	/* !_PCI_TWAREG_H_ */
