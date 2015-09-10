/*	$NetBSD: v7fs_superblock.c,v 1.2 2011/07/18 21:51:49 apb Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: v7fs_superblock.c,v 1.2 2011/07/18 21:51:49 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/param.h>	/* errno */
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_endian.h"
#include "v7fs_superblock.h"
#include "v7fs_inode.h"
#include "v7fs_datablock.h"

#ifdef V7FS_SUPERBLOCK_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#define	DPRINTF_(fmt, args...)	printf(fmt, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#define	DPRINTF_(fmt, args...)	((void)0)
#endif

static void v7fs_superblock_endian_convert(struct v7fs_self *,
    struct v7fs_superblock *, struct v7fs_superblock *);
static int v7fs_superblock_sanity(struct v7fs_self *);

/* Load superblock from disk. */
int
v7fs_superblock_load(struct v7fs_self *fs)
{
	struct v7fs_superblock *disksb;
	void *buf;
	int error;

	if (!(buf = scratch_read(fs, V7FS_SUPERBLOCK_SECTOR)))
		return EIO;
	disksb = (struct v7fs_superblock *)buf;
	v7fs_superblock_endian_convert(fs, &fs->superblock, disksb);
	scratch_free(fs, buf);

	if ((error = v7fs_superblock_sanity(fs)))
		return error;

	return 0;
}

/* Writeback superblock to disk. */
int
v7fs_superblock_writeback(struct v7fs_self *fs)
{
	struct v7fs_superblock *memsb = &fs->superblock;
	struct v7fs_superblock *disksb;
	void *buf;
	int error = 0;

	if (!memsb->modified)
		return 0;

	if (!(buf = scratch_read(fs, V7FS_SUPERBLOCK_SECTOR)))
		return EIO;
	disksb = (struct v7fs_superblock *)buf;
	v7fs_superblock_endian_convert(fs, disksb, memsb);
	if (!fs->io.write(fs->io.cookie, buf, V7FS_SUPERBLOCK_SECTOR))
		error = EIO;
	scratch_free(fs, buf);

	memsb->modified = 0;
	DPRINTF("done. %d\n", error);

	return error;
}

/* Check endian mismatch. */
static int
v7fs_superblock_sanity(struct v7fs_self *fs)
{
	const struct v7fs_superblock *sb = &fs->superblock;
	void *buf = 0;

	if ((sb->volume_size < 128) || /* smaller than 64KB. */
	    (sb->datablock_start_sector > sb->volume_size) ||
	    (sb->nfreeinode > V7FS_MAX_FREEINODE) ||
	    (sb->nfreeblock > V7FS_MAX_FREEBLOCK) ||
	    (sb->update_time < 0) ||
	    (sb->total_freeblock > sb->volume_size) ||
	    ((sb->nfreeinode == 0) && (sb->nfreeblock == 0) &&
		(sb->total_freeblock == 0) && (sb->total_freeinode == 0)) ||
	    (!(buf = scratch_read(fs, sb->volume_size - 1)))) {
		DPRINTF("invalid super block.\n");
		return EINVAL;
	}
	if (buf)
		scratch_free(fs, buf);

	return 0;
}

/* Fill free block to superblock cache. */
int
v7fs_freeblock_update(struct v7fs_self *fs, v7fs_daddr_t blk)
{
	/* Assume superblock is locked by caller. */
	struct v7fs_superblock *sb = &fs->superblock;
	struct v7fs_freeblock *fb;
	void *buf;
	int error;

	/* Read next freeblock table from disk. */
	if (!datablock_number_sanity(fs, blk) || !(buf = scratch_read(fs, blk)))
		return EIO;

	/* Update in-core superblock freelist. */
	fb = (struct v7fs_freeblock *)buf;
	if ((error = v7fs_freeblock_endian_convert(fs, fb))) {
		scratch_free(fs, buf);
		return error;
	}
	DPRINTF("freeblock table#%d, nfree=%d\n", blk, fb->nfreeblock);

	memcpy(sb->freeblock, fb->freeblock, sizeof(blk) * fb->nfreeblock);
	sb->nfreeblock = fb->nfreeblock;
	sb->modified = true;
	scratch_free(fs, buf);

	return 0;
}

int
v7fs_freeblock_endian_convert(struct v7fs_self *fs __unused,
    struct v7fs_freeblock *fb __unused)
{
#ifdef V7FS_EI
	int i;
	int16_t nfree;

	nfree = V7FS_VAL16(fs, fb->nfreeblock);
	if (nfree <= 0 || nfree > V7FS_MAX_FREEBLOCK) {
		DPRINTF("invalid freeblock list. %d (max=%d)\n", nfree,
		    V7FS_MAX_FREEBLOCK);
		return ENOSPC;
	}
	fb->nfreeblock = nfree;

	for (i = 0; i < nfree; i++) {
		fb->freeblock[i] = V7FS_VAL32(fs, fb->freeblock[i]);
	}
#endif /* V7FS_EI */

	return 0;
}

/* Fill free inode to superblock cache. */
int
v7fs_freeinode_update(struct v7fs_self *fs)
{
	/* Assume superblock is locked by caller. */
	struct v7fs_superblock *sb = &fs->superblock;
	v7fs_ino_t *freeinode = sb->freeinode;
	size_t i, j, k;
	v7fs_ino_t ino;

	/* Loop over all inode list. */
	for (i = V7FS_ILIST_SECTOR, ino = 1/* inode start from 1*/, k = 0;
	    i < sb->datablock_start_sector; i++) {
		struct v7fs_inode_diskimage *di;
		void *buf;
		if (!(buf = scratch_read(fs, i))) {
			DPRINTF("block %zu I/O error.\n", i);
			ino += V7FS_INODE_PER_BLOCK;
			continue;
		}
		di = (struct v7fs_inode_diskimage *)buf;

		for (j = 0;
		    (j < V7FS_INODE_PER_BLOCK) && (k < V7FS_MAX_FREEINODE);
		    j++, di++, ino++) {
			if (v7fs_inode_allocated(di))
				continue;
			DPRINTF("free inode%d\n", ino);
			freeinode[k++] = ino;
		}
		scratch_free(fs, buf);
	}
	sb->nfreeinode = k;

	return 0;
}

static void
v7fs_superblock_endian_convert(struct v7fs_self *fs __unused,
    struct v7fs_superblock *to,  struct v7fs_superblock *from)
{
#ifdef V7FS_EI
#define	conv16(m)	(to->m = V7FS_VAL16(fs, from->m))
#define	conv32(m)	(to->m = V7FS_VAL32(fs, from->m))
	int i;

	conv16(datablock_start_sector);
	conv32(volume_size);
	conv16(nfreeblock);
	v7fs_daddr_t *dfrom = from->freeblock;
	v7fs_daddr_t *dto = to->freeblock;
	for (i = 0; i < V7FS_MAX_FREEBLOCK; i++, dfrom++, dto++)
		*dto = V7FS_VAL32(fs, *dfrom);

	conv16(nfreeinode);
	v7fs_ino_t *ifrom = from->freeinode;
	v7fs_ino_t *ito = to->freeinode;
	for (i = 0; i < V7FS_MAX_FREEINODE; i++, ifrom++, ito++)
		*ito = V7FS_VAL16(fs, *ifrom);

	conv32(update_time);
	conv32(total_freeblock);
	conv16(total_freeinode);
#undef conv16
#undef conv32
#else /* V7FS_EI */
	memcpy(to, from , sizeof(*to));
#endif /* V7FS_EI */
}
