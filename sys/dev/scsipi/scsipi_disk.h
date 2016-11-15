/*	$NetBSD: scsipi_disk.h,v 1.21 2007/12/25 18:33:42 perry Exp $	*/

/*
 * SCSI and SCSI-like interfaces description
 */

/*
 * Some lines of this file come from a file of the name "scsi.h"
 * distributed by OSF as part of mach2.5,
 *  so the following disclaimer has been kept.
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 *
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Largely written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
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
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#ifndef _DEV_SCSIPI_SCSIPI_DISK_H_
#define _DEV_SCSIPI_SCSIPI_DISK_H_

/*
 * SCSI command format
 */

#define	READ_10			0x28
#define WRITE_10		0x2a
struct scsipi_rw_10 {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRWB_RELADDR	0x01	/* obsolete */
#define	SRWB_FUA_NV	0x02	/* force unit access non-volatile cache */
#define	SRWB_FUA	0x08	/* force unit access */
#define	SRWB_DPO	0x10	/* disable page out */
#define	SRWB_PROTECT(x) ((x) << 5)
	u_int8_t addr[4];
	u_int8_t reserved;
	u_int8_t length[2];
	u_int8_t control;
} __packed;

#define	READ_12			0xa8
#define	WRITE_12		0xaa
struct scsipi_rw_12 {
	u_int8_t opcode;
	u_int8_t byte2;		/* see scsipi_rw_big bits */
	u_int8_t addr[4];
	u_int8_t length[4];
	u_int8_t byte11;
	u_int8_t control;
} __packed;

#define	READ_16			0x88
#define	WRITE_16		0x8a
struct scsipi_rw_16 {
	u_int8_t opcode;
	u_int8_t byte2;		/* see scsipi_rw_big bits */
	u_int8_t addr[8];
	u_int8_t length[4];
	u_int8_t byte15;
	u_int8_t control;
} __packed;

#define	READ_CAPACITY_10	0x25
struct scsipi_read_capacity_10 {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t unused[3];
	u_int8_t control;
} __packed;

/* DATAs definitions for the above commands */

struct scsipi_read_capacity_10_data {
	u_int8_t addr[4];
	u_int8_t length[4];
} __packed;

#define	READ_CAPACITY_16	0x9e	/* really SERVICE ACTION IN */
struct scsipi_read_capacity_16 {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRC16_SERVICE_ACTION	0x10
	u_int8_t addr[8];
	u_int8_t len[4];
	u_int8_t byte15;
#define	SRC16_PMI		0x01
	u_int8_t control;
} __packed;

struct scsipi_read_capacity_16_data {
	u_int8_t addr[8];
	u_int8_t length[4];
	u_int8_t byte13;
#define	SRC16D_PROT_EN		0x01
#define	SRC16D_RTO_EN		0x02
	u_int8_t reserved[19];
} __packed;

/* XXX SBC-2 says this is vendor-specific */
#define READ_FORMAT_CAPACITIES	0x23
struct scsipi_read_format_capacities {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t reserved1[5];
	u_int8_t length[2];
	u_int8_t reserved2[3];
} __packed;

struct scsipi_capacity_list_header {
	u_int8_t reserved[3];
	u_int8_t length;
} __packed;

struct scsipi_capacity_descriptor {
	u_int8_t nblks[4];
	u_int8_t byte5;
	u_int8_t blklen[3];
} __packed;

/* codes only valid in the current/maximum capacity descriptor */
#define	SCSIPI_CAP_DESC_CODE_MASK		0x3
#define	SCSIPI_CAP_DESC_CODE_RESERVED		0x0
#define	SCSIPI_CAP_DESC_CODE_UNFORMATTED	0x1
#define	SCSIPI_CAP_DESC_CODE_FORMATTED		0x2
#define	SCSIPI_CAP_DESC_CODE_NONE		0x3

#endif /* _DEV_SCSIPI_SCSIPI_DISK_H_ */
