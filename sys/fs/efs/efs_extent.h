/*	$NetBSD: efs_extent.h,v 1.3 2007/07/04 19:24:09 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
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

/*
 * EFS extent descriptor format and sundry.
 *
 * See IRIX inode(4)
 */

#ifndef _FS_EFS_EFS_EXTENT_H_
#define _FS_EFS_EFS_EXTENT_H_

/*
 * EFS on-disk extent descriptor (8 bytes)
 *
 * SGI smushed this structure's members into bit fields, but we have to
 * be a little more portable. Therefore we use the efs_extent (see below)
 * type for in-core manipulation and convert immediately to and from disk.
 */
struct efs_dextent {
	union {
		uint64_t ex_magic:8,	/* magic number (always 0) */
			 ex_bn:24,	/* bb number in filesystem */
			 ex_length:8,	/* length of extent (in bb) */
			 ex_offset:24;	/* logical file offset (in bb) */
		uint8_t  bytes[8];
		uint32_t words[2];
	} ex_muddle;
} __packed;
#define ex_bytes ex_muddle.bytes
#define ex_words ex_muddle.words

/*
 * In-core, unsquished representation of an extent.
 */
struct efs_extent {
	uint8_t  ex_magic;
	uint32_t ex_bn;			/* NB: only 24 bits on disk */
	uint8_t  ex_length;
	uint32_t ex_offset;		/* NB: only 24 bits on disk */
};

#define EFS_EXTENT_MAGIC	0
#define EFS_EXTENT_BN_MASK	0x00ffffff
#define EFS_EXTENT_OFFSET_MASK	0x00ffffff

#define EFS_EXTENTS_PER_BB	(EFS_BB_SIZE / sizeof(struct efs_dextent))

#endif /* !_FS_EFS_EFS_EXTENT_H_ */
