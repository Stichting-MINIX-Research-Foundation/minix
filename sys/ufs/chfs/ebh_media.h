/*	$NetBSD: ebh_media.h,v 1.1 2011/11/24 15:51:32 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2009 Ferenc Havasi <havasi@inf.u-szeged.hu>
 * Copyright (C) 2009 Zoltan Sogor <weth@inf.u-szeged.hu>
 * Copyright (C) 2009 David Tengeri <dtengeri@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef EBH_MEDIA_H_
#define EBH_MEDIA_H_

#ifndef _LE_TYPES
#define _LE_TYPES
typedef uint16_t le16;
typedef uint32_t le32;
typedef uint64_t le64;
#endif

/*****************************************************************************/
/*			EBH specific structures				     */
/*****************************************************************************/
#define CHFS_MAGIC_BITMASK 0x53454452

#define CHFS_LID_NOT_DIRTY_BIT  0x80000000
#define CHFS_LID_DIRTY_BIT_MASK 0x7fffffff

/* sizeof(crc) + sizeof(lid) */
#define CHFS_INVALIDATE_SIZE 8

/* Size of magic + crc_ec +  erase_cnt */
#define CHFS_EB_EC_HDR_SIZE sizeof(struct chfs_eb_ec_hdr)
/* Size of NOR eraseblock header */
#define CHFS_EB_HDR_NOR_SIZE sizeof(struct chfs_nor_eb_hdr)
/* Size of NAND eraseblock header */
#define CHFS_EB_HDR_NAND_SIZE sizeof(struct chfs_nand_eb_hdr)

/*
 * chfs_eb_ec_hdr - erase counter header of eraseblock
 * @magic: filesystem magic
 * @crc_ec: CRC32 sum of erase counter
 * @erase_cnt: erase counter
 *
 * This structure holds the erasablock description information.
 * This will be written to the beginning of the eraseblock.
 *
 */
struct chfs_eb_ec_hdr {
	le32 magic;
	le32 crc_ec;
	le32 erase_cnt;
} __packed;

/**
 * struct chfs_nor_eb_hdr - eraseblock header on NOR flash
 * @crc: CRC32 sum
 * @lid: logical identifier
 *
 * @lid contains the logical block reference but only the first 31 bit (0-30) is
 * used. The 32th bit is for marking a lid dirty (marked for recovery purposes).
 * If a new eraseblock is succesfully assigned with the same lid then the lid of
 * the old one is zeroed. If power failure happened during this operation then
 * the recovery detects that there is two eraseblock with the same lid, but one
 * of them is marked (the old one).
 *
 * Invalidated eraseblock header means that the @crc and @lid is set to 0.
 */
struct chfs_nor_eb_hdr {
	le32 crc;
	le32 lid;
} __packed;

/**
 * struct chfs_nand_eb_hdr - eraseblock header on NAND flash
 * @crc: CRC32 sum
 * @lid: logical identifier
 * @serial: layout of the lid
 *
 * @serial is an unique number. Every eraseblock header on NAND flash has its
 * own serial. If there are two eraseblock on the flash referencing to the same
 * logical eraseblock, the one with bigger serial is the newer.
 */
struct chfs_nand_eb_hdr {
	le32 crc;
	le32 lid;
	le64 serial;
} __packed;

#endif /* EBH_MEDIA_H_ */
