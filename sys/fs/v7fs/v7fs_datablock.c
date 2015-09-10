/*	$NetBSD: v7fs_datablock.c,v 1.5 2011/08/14 09:02:07 apb Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_datablock.c,v 1.5 2011/08/14 09:02:07 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/param.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_endian.h"
#include "v7fs_inode.h"
#include "v7fs_datablock.h"
#include "v7fs_superblock.h"

#ifdef V7FS_DATABLOCK_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

static int v7fs_datablock_deallocate(struct v7fs_self *, v7fs_daddr_t);
static int v7fs_loop1(struct v7fs_self *, v7fs_daddr_t, size_t *,
    int (*)(struct v7fs_self *, void *, v7fs_daddr_t, size_t), void *);
static int v7fs_loop2(struct v7fs_self *, v7fs_daddr_t, size_t *,
    int (*)(struct v7fs_self *, void *, v7fs_daddr_t, size_t), void *);
static v7fs_daddr_t v7fs_link(struct v7fs_self *, v7fs_daddr_t, int);
static v7fs_daddr_t v7fs_add_leaf(struct v7fs_self *, v7fs_daddr_t, int);
static v7fs_daddr_t v7fs_unlink(struct v7fs_self *, v7fs_daddr_t, int);
static v7fs_daddr_t v7fs_remove_leaf(struct v7fs_self *, v7fs_daddr_t, int);
static v7fs_daddr_t v7fs_remove_self(struct v7fs_self *, v7fs_daddr_t);

#ifdef V7FS_DATABLOCK_DEBUG
void daddr_map_dump(const struct v7fs_daddr_map *);
#else
#define	daddr_map_dump(x)	((void)0)
#endif

bool
datablock_number_sanity(const struct v7fs_self *fs, v7fs_daddr_t blk)
{
	const struct v7fs_superblock *sb = &fs->superblock;
	bool ok = (blk >= sb->datablock_start_sector) &&
	    (blk < sb->volume_size);

#ifdef V7FS_DATABLOCK_DEBUG
	if (!ok) {
		DPRINTF("Bad data block #%d\n", blk);
	}
#endif

	return ok;
}

int
v7fs_datablock_allocate(struct v7fs_self *fs, v7fs_daddr_t *block_number)
{
	struct v7fs_superblock *sb = &fs->superblock;
	v7fs_daddr_t blk;
	int error = 0;

	*block_number = 0;
	SUPERB_LOCK(fs);
	do {
		if (!sb->total_freeblock) {
			DPRINTF("free block exhausted!!!\n");
			SUPERB_UNLOCK(fs);
			return ENOSPC;
		}

		/* Get free block from superblock cache. */
		blk = sb->freeblock[--sb->nfreeblock];
		sb->total_freeblock--;
		sb->modified = 1;

		/* If nfreeblock is zero, it block is next freeblock link. */
		if (sb->nfreeblock == 0) {
			if ((error = v7fs_freeblock_update(fs, blk))) {
				DPRINTF("no freeblock!!!\n");
				SUPERB_UNLOCK(fs);
				return error;
			}
			/* This freeblock link is no longer required. */
			/* use as data block. */
		}
	} while (!datablock_number_sanity(fs, blk)); /* skip bogus block. */
	SUPERB_UNLOCK(fs);

	DPRINTF("Get freeblock %d\n", blk);
	/* Zero clear datablock. */
	void *buf;
	if (!(buf = scratch_read(fs, blk)))
		return EIO;
	memset(buf, 0, V7FS_BSIZE);
	if (!fs->io.write(fs->io.cookie, buf, blk))
		error = EIO;
	scratch_free(fs, buf);

	if (error == 0)
		*block_number = blk;

	return error;
}

static int
v7fs_datablock_deallocate(struct v7fs_self *fs, v7fs_daddr_t blk)
{
	struct v7fs_superblock *sb = &fs->superblock;
	void *buf;
	int error = 0;

	if (!datablock_number_sanity(fs, blk))
		return EIO;

	/* Add to in-core freelist. */
	SUPERB_LOCK(fs);
	if (sb->nfreeblock < V7FS_MAX_FREEBLOCK) {
		sb->freeblock[sb->nfreeblock++] = blk;
		sb->total_freeblock++;
		sb->modified = 1;
		DPRINTF("n_freeblock=%d\n", sb->total_freeblock);
		SUPERB_UNLOCK(fs);
		return 0;
	}

	/* No space to push. */
	/* Make this block to freeblock list.and current cache moved to this. */
	if (!(buf = scratch_read(fs, blk))) {
		SUPERB_UNLOCK(fs);
		return EIO;	/* Fatal */
	}

	struct v7fs_freeblock *fb = (struct v7fs_freeblock *)buf;
	fb->nfreeblock = V7FS_MAX_FREEBLOCK;
	int i;
	for (i = 0; i < V7FS_MAX_FREEBLOCK; i++)
		fb->freeblock[i] = V7FS_VAL32(fs, sb->freeblock[i]);

	if (!fs->io.write(fs->io.cookie, (uint8_t *)fb, blk)) {
		error =  EIO;	/* Fatal */
	} else {
		/* Link. on next allocate, this block is used as datablock, */
		/* and swap outed freeblock list is restored. */
		sb->freeblock[0] = blk;
		sb->nfreeblock = 1;
		sb->total_freeblock++;
		sb->modified = 1;
		DPRINTF("n_freeblock=%d\n", sb->total_freeblock);
	}
	SUPERB_UNLOCK(fs);
	scratch_free(fs, buf);

	return error;
}

int
v7fs_datablock_addr(size_t sz, struct v7fs_daddr_map *map)
{
#define	NIDX		V7FS_DADDR_PER_BLOCK
#define	DIRECT_SZ	(V7FS_NADDR_DIRECT * V7FS_BSIZE)
#define	IDX1_SZ		(NIDX * V7FS_BSIZE)
#define	IDX2_SZ		(NIDX * NIDX * V7FS_BSIZE)
#define	ROUND(x, a)	((((x) + ((a) - 1)) & ~((a) - 1)))
	if (!sz) {
		map->level = 0;
		map->index[0] = 0;
		return 0;
	}

	sz = V7FS_ROUND_BSIZE(sz);

	/* Direct */
	if (sz <= DIRECT_SZ) {
		map->level = 0;
		map->index[0] = (sz >> V7FS_BSHIFT) - 1;
		return 0;
	}
	/* Index 1 */
	sz -= DIRECT_SZ;

	if (sz <= IDX1_SZ) {
		map->level = 1;
		map->index[0] = (sz >> V7FS_BSHIFT) - 1;
		return 0;
	}
	sz -= IDX1_SZ;

	/* Index 2 */
	if (sz <= IDX2_SZ) {
		map->level = 2;
		map->index[0] = ROUND(sz, IDX1_SZ) / IDX1_SZ - 1;
		map->index[1] = ((sz - (map->index[0] * IDX1_SZ)) >>
		    V7FS_BSHIFT) - 1;
		return 0;
	}
	sz -= IDX2_SZ;

	/* Index 3 */
	map->level = 3;
	map->index[0] = ROUND(sz, IDX2_SZ) / IDX2_SZ - 1;
	sz -= map->index[0] * IDX2_SZ;
	map->index[1] = ROUND(sz, IDX1_SZ) / IDX1_SZ - 1;
	sz -= map->index[1] * IDX1_SZ;
	map->index[2] = (sz >> V7FS_BSHIFT) - 1;

	return map->index[2] >= NIDX ? ENOSPC : 0;
}

int
v7fs_datablock_foreach(struct v7fs_self *fs, struct v7fs_inode *p,
    int (*func)(struct v7fs_self *, void *, v7fs_daddr_t, size_t), void *ctx)
{
	size_t i;
	v7fs_daddr_t blk, blk2;
	size_t filesize;
	bool last;
	int ret;

	if (!(filesize = v7fs_inode_filesize(p)))
		return V7FS_ITERATOR_END;
#ifdef V7FS_DATABLOCK_DEBUG
	size_t sz = filesize;
#endif

	/* Direct */
	for (i = 0; i < V7FS_NADDR_DIRECT; i++, filesize -= V7FS_BSIZE) {
		blk = p->addr[i];
		if (!datablock_number_sanity(fs, blk)) {
			DPRINTF("inode#%d direct=%zu filesize=%zu\n",
			    p->inode_number, i, sz);
			return EIO;
		}

		last = filesize <= V7FS_BSIZE;
		if ((ret = func(fs, ctx, blk, last ? filesize : V7FS_BSIZE)))
			return ret;
		if (last)
			return V7FS_ITERATOR_END;
	}

	/* Index 1 */
	blk = p->addr[V7FS_NADDR_INDEX1];
	if (!datablock_number_sanity(fs, blk))
		return EIO;

	if ((ret = v7fs_loop1(fs, blk, &filesize, func, ctx)))
		return ret;

	/* Index 2 */
	blk = p->addr[V7FS_NADDR_INDEX2];
	if (!datablock_number_sanity(fs, blk))
		return EIO;

	if ((ret = v7fs_loop2(fs, blk, &filesize, func, ctx)))
		return ret;

	/* Index 3 */
	blk = p->addr[V7FS_NADDR_INDEX3];
	if (!datablock_number_sanity(fs, blk))
		return EIO;

	for (i = 0; i < V7FS_DADDR_PER_BLOCK; i++) {
		blk2 = v7fs_link(fs, blk, i);
		if (!datablock_number_sanity(fs, blk))
			return EIO;

		if ((ret = v7fs_loop2(fs, blk2, &filesize, func, ctx)))
			return ret;
	}

	return EFBIG;
}

static int
v7fs_loop2(struct v7fs_self *fs, v7fs_daddr_t listblk, size_t *filesize,
    int (*func)(struct v7fs_self *, void *, v7fs_daddr_t, size_t), void *ctx)
{
	v7fs_daddr_t blk;
	int ret;
	size_t j;

	for (j = 0; j < V7FS_DADDR_PER_BLOCK; j++) {
		blk = v7fs_link(fs, listblk, j);
		if (!datablock_number_sanity(fs, blk))
			return EIO;
		if ((ret = v7fs_loop1(fs, blk, filesize, func, ctx)))
			return ret;
	}

	return 0;
}

static int
v7fs_loop1(struct v7fs_self *fs, v7fs_daddr_t listblk, size_t *filesize,
    int (*func)(struct v7fs_self *, void *, v7fs_daddr_t, size_t), void *ctx)
{
	v7fs_daddr_t blk;
	bool last;
	int ret;
	size_t k;

	for (k = 0; k < V7FS_DADDR_PER_BLOCK; k++, *filesize -= V7FS_BSIZE) {
		blk = v7fs_link(fs, listblk, k);
		if (!datablock_number_sanity(fs, blk))
			return EIO;
		last = *filesize <= V7FS_BSIZE;
		if ((ret = func(fs, ctx, blk, last ? *filesize : V7FS_BSIZE)))
			return ret;
		if (last)
			return V7FS_ITERATOR_END;
	}

	return 0;
}

v7fs_daddr_t
v7fs_datablock_last(struct v7fs_self *fs, struct v7fs_inode *inode,
    v7fs_off_t ofs)
{
	struct v7fs_daddr_map map;
	v7fs_daddr_t blk = 0;
	v7fs_daddr_t *addr = inode->addr;

	/* Inquire last data block location. */
	if (v7fs_datablock_addr(ofs, &map) != 0)
		return 0;

	switch (map.level)
	{
	case 0: /*Direct */
		blk = inode->addr[map.index[0]];
		break;
	case 1: /*Index1 */
		blk = v7fs_link(fs, addr[V7FS_NADDR_INDEX1], map.index[0]);
		break;
	case 2: /*Index2 */
		blk = v7fs_link(fs, v7fs_link(fs,
		    addr[V7FS_NADDR_INDEX2], map.index[0]), map.index[1]);
		break;
	case 3: /*Index3 */
		blk = v7fs_link(fs, v7fs_link(fs, v7fs_link(fs,
		    addr[V7FS_NADDR_INDEX3], map.index[0]), map.index[1]),
		    map.index[2]);
		break;
	}

	return blk;
}

int
v7fs_datablock_expand(struct v7fs_self *fs, struct v7fs_inode *inode, size_t sz)
{
	size_t old_filesize = inode->filesize;
	size_t new_filesize = old_filesize + sz;
	struct v7fs_daddr_map oldmap, newmap;
	v7fs_daddr_t blk, idxblk;
	int error;
	v7fs_daddr_t old_nblk = V7FS_ROUND_BSIZE(old_filesize) >> V7FS_BSHIFT;
	v7fs_daddr_t new_nblk = V7FS_ROUND_BSIZE(new_filesize) >> V7FS_BSHIFT;

	if (old_nblk == new_nblk) {
		inode->filesize += sz;
		v7fs_inode_writeback(fs, inode);
		return 0; /* no need to expand. */
	}
	struct v7fs_inode backup = *inode;
	v7fs_daddr_t required_blk = new_nblk - old_nblk;

	DPRINTF("%zu->%zu, required block=%d\n", old_filesize, new_filesize,
	    required_blk);

	v7fs_datablock_addr(old_filesize, &oldmap);
	v7fs_daddr_t i;
	for (i = 0; i < required_blk; i++) {
		v7fs_datablock_addr(old_filesize + (i+1) * V7FS_BSIZE, &newmap);
		daddr_map_dump(&oldmap);
		daddr_map_dump(&newmap);

		if (oldmap.level != newmap.level) {
			/* Allocate index area */
			if ((error = v7fs_datablock_allocate(fs, &idxblk)))
				return error;

			switch (newmap.level) {
			case 1:
				DPRINTF("0->1\n");
				inode->addr[V7FS_NADDR_INDEX1] = idxblk;
				blk = v7fs_add_leaf(fs, idxblk, 0);
				break;
			case 2:
				DPRINTF("1->2\n");
				inode->addr[V7FS_NADDR_INDEX2] = idxblk;
				blk = v7fs_add_leaf(fs, v7fs_add_leaf(fs,
				    idxblk, 0), 0);
				break;
			case 3:
				DPRINTF("2->3\n");
				inode->addr[V7FS_NADDR_INDEX3] = idxblk;
				blk = v7fs_add_leaf(fs, v7fs_add_leaf(fs,
				    v7fs_add_leaf(fs, idxblk, 0), 0), 0);
				break;
			}
		} else {
			switch (newmap.level) {
			case 0:
				if ((error = v7fs_datablock_allocate(fs, &blk)))
					return error;
				inode->addr[newmap.index[0]] = blk;
				DPRINTF("direct index %d = blk%d\n",
				    newmap.index[0], blk);
				break;
			case 1:
				idxblk = inode->addr[V7FS_NADDR_INDEX1];
				blk = v7fs_add_leaf(fs, idxblk,
				    newmap.index[0]);
				break;
			case 2:
				idxblk = inode->addr[V7FS_NADDR_INDEX2];
				if (oldmap.index[0] != newmap.index[0]) {
					v7fs_add_leaf(fs, idxblk,
					    newmap.index[0]);
				}
				blk = v7fs_add_leaf(fs, v7fs_link(fs,idxblk,
				    newmap.index[0]), newmap.index[1]);
				break;
			case 3:
				idxblk = inode->addr[V7FS_NADDR_INDEX3];

				if (oldmap.index[0] != newmap.index[0]) {
					v7fs_add_leaf(fs, idxblk,
					    newmap.index[0]);
				}

				if (oldmap.index[1] != newmap.index[1]) {
					v7fs_add_leaf(fs, v7fs_link(fs, idxblk,
					    newmap.index[0]), newmap.index[1]);
				}
				blk = v7fs_add_leaf(fs, v7fs_link(fs,
				    v7fs_link(fs, idxblk, newmap.index[0]),
				    newmap.index[1]), newmap.index[2]);
				break;
			}
		}
		if (!blk) {
			*inode = backup; /* structure copy; */
			return ENOSPC;
		}
		oldmap = newmap;
	}
	inode->filesize += sz;
	v7fs_inode_writeback(fs, inode);

	return 0;
}

static v7fs_daddr_t
v7fs_link(struct v7fs_self *fs, v7fs_daddr_t listblk, int n)
{
	v7fs_daddr_t *list;
	v7fs_daddr_t blk;
	void *buf;

	if (!datablock_number_sanity(fs, listblk))
		return 0;
	if (!(buf = scratch_read(fs, listblk)))
		return 0;
	list = (v7fs_daddr_t *)buf;
	blk = V7FS_VAL32(fs, list[n]);
	scratch_free(fs, buf);

	if (!datablock_number_sanity(fs, blk))
		return 0;

	return blk;
}

static v7fs_daddr_t
v7fs_add_leaf(struct v7fs_self *fs, v7fs_daddr_t up, int idx)
{
	v7fs_daddr_t newblk;
	v7fs_daddr_t *daddr_list;
	int error = 0;
	void *buf;

	if (!up)
		return 0;
	if (!datablock_number_sanity(fs, up))
		return 0;

	if ((error = v7fs_datablock_allocate(fs, &newblk)))
		return 0;
	if (!(buf = scratch_read(fs, up)))
		return 0;
	daddr_list = (v7fs_daddr_t *)buf;
	daddr_list[idx] = V7FS_VAL32(fs, newblk);
	if (!fs->io.write(fs->io.cookie, buf, up))
		newblk = 0;
	scratch_free(fs, buf);

	return newblk;
}

int
v7fs_datablock_contract(struct v7fs_self *fs, struct v7fs_inode *inode,
    size_t sz)
{
	size_t old_filesize = inode->filesize;
	size_t new_filesize = old_filesize - sz;
	struct v7fs_daddr_map oldmap, newmap;
	v7fs_daddr_t blk, idxblk;
	int error = 0;
	v7fs_daddr_t old_nblk = V7FS_ROUND_BSIZE(old_filesize) >> V7FS_BSHIFT;
	v7fs_daddr_t new_nblk = V7FS_ROUND_BSIZE(new_filesize) >> V7FS_BSHIFT;

	if (old_nblk == new_nblk) {
		inode->filesize -= sz;
		v7fs_inode_writeback(fs, inode);
		return 0; /* no need to contract; */
	}
	v7fs_daddr_t erase_blk = old_nblk - new_nblk;

	DPRINTF("%zu->%zu # of erased block=%d\n", old_filesize, new_filesize,
	    erase_blk);

	v7fs_datablock_addr(old_filesize, &oldmap);
	v7fs_daddr_t i;
	for (i = 0; i < erase_blk; i++) {
		v7fs_datablock_addr(old_filesize - (i+1) * V7FS_BSIZE, &newmap);

		if (oldmap.level != newmap.level) {
			switch (newmap.level) {
			case 0: /*1->0 */
				DPRINTF("1->0\n");
				idxblk = inode->addr[V7FS_NADDR_INDEX1];
				inode->addr[V7FS_NADDR_INDEX1] = 0;
				error = v7fs_datablock_deallocate(fs,
				    v7fs_remove_self(fs, idxblk));
				break;
			case 1: /*2->1 */
				DPRINTF("2->1\n");
				idxblk = inode->addr[V7FS_NADDR_INDEX2];
				inode->addr[V7FS_NADDR_INDEX2] = 0;
				error = v7fs_datablock_deallocate(fs,
				    v7fs_remove_self(fs, v7fs_remove_self(fs,
				    idxblk)));
				break;
			case 2:/*3->2 */
				DPRINTF("3->2\n");
				idxblk = inode->addr[V7FS_NADDR_INDEX3];
				inode->addr[V7FS_NADDR_INDEX3] = 0;
				error = v7fs_datablock_deallocate(fs,
				    v7fs_remove_self(fs, v7fs_remove_self(fs,
					v7fs_remove_self(fs, idxblk))));
				break;
			}
		} else {
			switch (newmap.level) {
			case 0:
				DPRINTF("[0] %d\n", oldmap.index[0]);
				blk = inode->addr[oldmap.index[0]];
				error = v7fs_datablock_deallocate(fs, blk);
				break;
			case 1:
				DPRINTF("[1] %d\n", oldmap.index[0]);
				idxblk = inode->addr[V7FS_NADDR_INDEX1];
				v7fs_remove_leaf(fs, idxblk, oldmap.index[0]);

				break;
			case 2:
				DPRINTF("[2] %d %d\n", oldmap.index[0],
				    oldmap.index[1]);
				idxblk = inode->addr[V7FS_NADDR_INDEX2];
				v7fs_remove_leaf(fs, v7fs_link(fs, idxblk,
				    oldmap.index[0]), oldmap.index[1]);
				if (oldmap.index[0] != newmap.index[0]) {
					v7fs_remove_leaf(fs, idxblk,
					    oldmap.index[0]);
				}
				break;
			case 3:
				DPRINTF("[2] %d %d %d\n", oldmap.index[0],
				    oldmap.index[1], oldmap.index[2]);
				idxblk = inode->addr[V7FS_NADDR_INDEX3];
				v7fs_remove_leaf(fs, v7fs_link(fs,
				    v7fs_link(fs, idxblk, oldmap.index[0]),
				    oldmap.index[1]), oldmap.index[2]);

				if (oldmap.index[1] != newmap.index[1])	{
					v7fs_remove_leaf(fs, v7fs_link(fs,
					    idxblk, oldmap.index[0]),
					    oldmap.index[1]);
				}
				if (oldmap.index[0] != newmap.index[0]) {
					v7fs_remove_leaf(fs, idxblk,
					    oldmap.index[0]);
				}
				break;
			}
		}
		oldmap = newmap;
	}
	inode->filesize -= sz;
	v7fs_inode_writeback(fs, inode);

	return error;
}

static v7fs_daddr_t
v7fs_unlink(struct v7fs_self *fs, v7fs_daddr_t idxblk, int n)
{
	v7fs_daddr_t *daddr_list;
	v7fs_daddr_t blk;
	void *buf;

	if (!(buf = scratch_read(fs, idxblk)))
		return 0;
	daddr_list = (v7fs_daddr_t *)buf;
	blk = V7FS_VAL32(fs, daddr_list[n]);
	daddr_list[n] = 0;
	fs->io.write(fs->io.cookie, buf, idxblk);
	scratch_free(fs, buf);

	return blk; /* unlinked block. */
}

static v7fs_daddr_t
v7fs_remove_self(struct v7fs_self *fs, v7fs_daddr_t up)
{
	v7fs_daddr_t down;

	if (!datablock_number_sanity(fs, up))
		return 0;

	/* At 1st, remove from datablock list. */
	down = v7fs_unlink(fs, up, 0);

	/* link self to freelist. */
	v7fs_datablock_deallocate(fs, up);

	return down;
}

static v7fs_daddr_t
v7fs_remove_leaf(struct v7fs_self *fs, v7fs_daddr_t up, int n)
{
	v7fs_daddr_t down;

	if (!datablock_number_sanity(fs, up))
		return 0;

	/* At 1st, remove from datablock list. */
	down = v7fs_unlink(fs, up, n);

	/* link leaf to freelist. */
	v7fs_datablock_deallocate(fs, down);

	return down;
}

int
v7fs_datablock_size_change(struct v7fs_self *fs, size_t newsz,
    struct v7fs_inode *inode)
{
	ssize_t diff = newsz - v7fs_inode_filesize(inode);
	int error = 0;

	if (diff > 0)
		error = v7fs_datablock_expand(fs, inode, diff);
	else if (diff < 0)
		error = v7fs_datablock_contract(fs, inode, -diff);

	return error;
}

#ifdef V7FS_DATABLOCK_DEBUG
void
daddr_map_dump(const struct v7fs_daddr_map *map)
{

	DPRINTF("level %d ", map->level);
	int m, n = !map->level ? 1 : map->level;
	for (m = 0; m < n; m++)
		printf("[%d]", map->index[m]);
	printf("\n");
}
#endif
