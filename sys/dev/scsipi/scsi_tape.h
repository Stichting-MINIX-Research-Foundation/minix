/*	$NetBSD: scsi_tape.h,v 1.25 2008/04/28 20:23:57 martin Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*
 * SCSI tape interface description
 */

/*
 * SCSI command formats
 */

#define	READ			0x08
#define WRITE			0x0a
struct scsi_rw_tape {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRW_FIXED		0x01
	u_int8_t len[3];
	u_int8_t control;
};

#define	SPACE			0x11
struct scsi_space {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SS_CODE			0x03
#define SP_BLKS			0x00
#define SP_FILEMARKS		0x01
#define SP_SEQ_FILEMARKS	0x02
#define	SP_EOM			0x03
	u_int8_t number[3];
	u_int8_t control;
};

#define	WRITE_FILEMARKS		0x10
struct scsi_write_filemarks {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t number[3];
	u_int8_t control;
};

#define REWIND			0x01
struct scsi_rewind {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SR_IMMED		0x01
	u_int8_t unused[3];
	u_int8_t control;
};

#define LOAD			0x1b
struct scsi_load {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SL_IMMED		0x01
	u_int8_t unused[2];
	u_int8_t how;
#define LD_UNLOAD		0x00
#define LD_LOAD			0x01
#define LD_RETENSION		0x02
	u_int8_t control;
};

#define	ERASE			0x19
struct scsi_erase {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SE_LONG			0x01
#define	SE_IMMED		0x02
	u_int8_t unused[3];
	u_int8_t control;
};

#define	READ_BLOCK_LIMITS	0x05
struct scsi_block_limits {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_block_limits_data {
	u_int8_t reserved;
	u_int8_t max_length[3];		/* Most significant */
	u_int8_t min_length[2];		/* Most significant */
};

/* See SCSI-II spec 9.3.3.1 */
struct scsi_tape_dev_conf_page {
	u_int8_t pagecode;	/* 0x10 */
	u_int8_t pagelength;	/* 0x0e */
	u_int8_t byte2;
#define	SMT_CAP			0x40	/* change active partition */
#define	SMT_CAF			0x20	/* change active format */
#define	SMT_AFMASK		0x1f	/* active format mask */
	u_int8_t active_partition;
	u_int8_t wb_full_ratio;
	u_int8_t rb_empty_ratio;
	u_int8_t wrdelay_time[2];
	u_int8_t byte8;
#define	SMT_DBR			0x80	/* data buffer recovery */
#define	SMT_BIS			0x40	/* block identifiers supported */
#define	SMT_RSMK		0x20	/* report setmarks */
#define	SMT_AVC			0x10	/* automatic velocity control */
#define SMT_SOCF_MASK		0xc0	/* stop on consecutive formats */
#define	SMT_RBO			0x20	/* recover buffer order */
#define	SMT_REW			0x10	/* report early warning */
	u_int8_t gap_size;
	u_int8_t byte10;
#define	SMT_EODDEFINED		0xe0	/* EOD defined */
#define	SMT_EEG			0x10	/* enable EOD generation */
#define	SMT_SEW			0x80	/* synchronize at early warning */
	u_int8_t ew_bufsize[3];
	u_int8_t sel_comp_alg;
#define	SMT_COMP_NONE		0x00
#define	SMT_COMP_DEFAULT	0x01
	u_int8_t reserved;
};

/* from SCSI-3: SSC-Rev10 (6/97) */
struct scsi_tape_dev_compression_page {
	u_int8_t pagecode;	/* 0x0f */
	u_int8_t pagelength;	/* 0x0e */
	u_int8_t dce_dcc;
#define	DCP_DCE			0x80	/* enable compression */
#define	DCP_DCC			0x40	/* compression capable */
	u_int8_t dde_red;
#define	DCP_DDE			0x80	/* enable decompression */
/* There's a lot of gup about bits 5,6 for reporting exceptions */
/* in transitions between compressed and uncompressed data- but */
/* mostly we want the default (0), which is to report a MEDIUM	*/
/* ERROR when a read transitions into data that can't be de-	*/
/* compressed */
	u_int8_t comp_alg[4];		/* compression algorithm */
	u_int8_t decomp_alg[4];		/* de-"" */
	u_int8_t reserved[4];
};

/* defines for the device specific byte in the mode select/sense header */
#define	SMH_DSP_SPEED		0x0F
#define	SMH_DSP_BUFF_MODE	0x70
#define	SMH_DSP_BUFF_MODE_OFF	0x00
#define	SMH_DSP_BUFF_MODE_ON	0x10
#define	SMH_DSP_BUFF_MODE_MLTI	0x20
#define	SMH_DSP_WRITE_PROT	0x80

#define	READ_POSITION	0x34
struct scsi_tape_read_position {
	u_int8_t opcode;
	u_int8_t byte1;			/* set LSB to read hardware block pos */
	u_int8_t reserved[8];
};

#define	LOCATE		0x2B
struct scsi_tape_locate {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t reserved1;
	u_int8_t blkaddr[4];
	u_int8_t reserved2;
	u_int8_t partition;
	u_int8_t control;
};

#define	HALFINCH_800	0x01
#define	HALFINCH_1600	0x02
#define	HALFINCH_6250	0x03
#define	QIC_11		0x04
#define QIC_24		0x05
#define QIC_120		0x0f
#define QIC_150		0x10
#define QIC_320		0x11
#define QIC_525		0x11
#define QIC_1320	0x12
#define DDS		0x13
#define QIC_3095	0x45
#define QIC_3220	0x47
