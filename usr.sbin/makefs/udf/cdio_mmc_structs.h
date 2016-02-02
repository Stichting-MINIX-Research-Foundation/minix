/* $NetBSD: cdio_mmc_structs.h,v 1.1 2013/08/05 18:44:16 reinoud Exp $ */

/*
 * Copyright (c) 2006, 2008, 2013 Reinoud Zandijk
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef _CDIO_MMC_EMU_H_
#define _CDIO_MMC_EMU_H_

#include <sys/types.h>

/*
 * MMC device abstraction interface.
 *
 * It gathers information from GET_CONFIGURATION, READ_DISCINFO,
 * READ_TRACKINFO, READ_TOC2, READ_CD_CAPACITY and GET_CONFIGURATION
 * SCSI/ATAPI calls regardless if its a legacy CD-ROM/DVD-ROM device or a MMC
 * standard recordable device.
 */
struct mmc_discinfo {
	uint16_t	mmc_profile;
	uint16_t	mmc_class;

	uint8_t		disc_state;
	uint8_t		last_session_state;
	uint8_t		bg_format_state;
	uint8_t		link_block_penalty;	/* in sectors		   */

	uint64_t	mmc_cur;		/* current MMC_CAPs        */
	uint64_t	mmc_cap;		/* possible MMC_CAPs       */

	uint32_t	disc_flags;		/* misc flags              */

	uint32_t	disc_id;
	uint64_t	disc_barcode;
	uint8_t		application_code;	/* 8 bit really            */

	uint8_t		unused1[3];		/* padding                 */

	uint32_t	last_possible_lba;	/* last leadout start adr. */
	uint32_t	sector_size;

	uint16_t	num_sessions;
	uint16_t	num_tracks;		/* derived */

	uint16_t	first_track;
	uint16_t	first_track_last_session;
	uint16_t	last_track_last_session;

	uint16_t	unused2;		/* padding/misc info resv. */

	uint16_t	reserved1[4];		/* MMC-5 track resources   */
	uint32_t	reserved2[3];		/* MMC-5 POW resources     */

	uint32_t	reserved3[8];		/* MMC-5+ */
};
#define MMCGETDISCINFO	_IOR('c', 28, struct mmc_discinfo)

#define MMC_CLASS_UNKN  0
#define MMC_CLASS_DISC	1
#define MMC_CLASS_CD	2
#define MMC_CLASS_DVD	3
#define MMC_CLASS_MO	4
#define MMC_CLASS_BD	5
#define MMC_CLASS_FILE	0xffff	/* emulation mode */

#define MMC_DFLAGS_BARCODEVALID	(1 <<  0)  /* barcode is present and valid   */
#define MMC_DFLAGS_DISCIDVALID  (1 <<  1)  /* discid is present and valid    */
#define MMC_DFLAGS_APPCODEVALID (1 <<  2)  /* application code valid         */
#define MMC_DFLAGS_UNRESTRICTED (1 <<  3)  /* restricted, then set app. code */

#define MMC_DFLAGS_FLAGBITS \
    "\10\1BARCODEVALID\2DISCIDVALID\3APPCODEVALID\4UNRESTRICTED"

#define MMC_CAP_SEQUENTIAL	(1 <<  0)  /* sequential writable only       */
#define MMC_CAP_RECORDABLE	(1 <<  1)  /* record-able; i.e. not static   */
#define MMC_CAP_ERASABLE	(1 <<  2)  /* drive can erase sectors        */
#define MMC_CAP_BLANKABLE	(1 <<  3)  /* media can be blanked           */
#define MMC_CAP_FORMATTABLE	(1 <<  4)  /* media can be formatted         */
#define MMC_CAP_REWRITABLE	(1 <<  5)  /* media can be rewritten         */
#define MMC_CAP_MRW		(1 <<  6)  /* Mount Rainier formatted        */
#define MMC_CAP_PACKET		(1 <<  7)  /* using packet recording         */
#define MMC_CAP_STRICTOVERWRITE	(1 <<  8)  /* only writes a packet at a time */
#define MMC_CAP_PSEUDOOVERWRITE (1 <<  9)  /* overwrite through replacement  */
#define MMC_CAP_ZEROLINKBLK	(1 << 10)  /* zero link block length capable */
#define MMC_CAP_HW_DEFECTFREE	(1 << 11)  /* hardware defect management     */

#define MMC_CAP_FLAGBITS \
    "\10\1SEQUENTIAL\2RECORDABLE\3ERASABLE\4BLANKABLE\5FORMATTABLE" \
    "\6REWRITABLE\7MRW\10PACKET\11STRICTOVERWRITE\12PSEUDOOVERWRITE" \
    "\13ZEROLINKBLK\14HW_DEFECTFREE"

#define MMC_STATE_EMPTY		0
#define MMC_STATE_INCOMPLETE	1
#define MMC_STATE_FULL		2
#define MMC_STATE_CLOSED	3

#define MMC_BGFSTATE_UNFORM	0
#define MMC_BGFSTATE_STOPPED	1
#define MMC_BGFSTATE_RUNNING	2
#define	MMC_BGFSTATE_COMPLETED	3


struct mmc_trackinfo {
	uint16_t	tracknr;	/* IN/OUT */
	uint16_t	sessionnr;

	uint8_t		track_mode;
	uint8_t		data_mode;

	uint16_t	flags;

	uint32_t	track_start;
	uint32_t	next_writable;
	uint32_t	free_blocks;
	uint32_t	packet_size;
	uint32_t	track_size;
	uint32_t	last_recorded;
};
#define MMCGETTRACKINFO	_IOWR('c', 29, struct mmc_trackinfo)

#define MMC_TRACKINFO_COPY		(1 <<  0)
#define MMC_TRACKINFO_DAMAGED		(1 <<  1)
#define MMC_TRACKINFO_FIXED_PACKET	(1 <<  2)
#define MMC_TRACKINFO_INCREMENTAL	(1 <<  3)
#define MMC_TRACKINFO_BLANK		(1 <<  4)
#define MMC_TRACKINFO_RESERVED		(1 <<  5)
#define MMC_TRACKINFO_NWA_VALID		(1 <<  6)
#define MMC_TRACKINFO_LRA_VALID		(1 <<  7)
#define MMC_TRACKINFO_DATA		(1 <<  8)
#define MMC_TRACKINFO_AUDIO		(1 <<  9)
#define MMC_TRACKINFO_AUDIO_4CHAN	(1 << 10)
#define MMC_TRACKINFO_PRE_EMPH		(1 << 11)

#define MMC_TRACKINFO_FLAGBITS \
    "\10\1COPY\2DAMAGED\3FIXEDPACKET\4INCREMENTAL\5BLANK" \
    "\6RESERVED\7NWA_VALID\10LRA_VALID\11DATA\12AUDIO" \
    "\13AUDIO_4CHAN\14PRE_EMPH"

#endif /* _CDIO_MMC_EMU_H_ */

