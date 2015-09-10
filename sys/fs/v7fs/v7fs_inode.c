/*	$NetBSD: v7fs_inode.c,v 1.2 2011/07/18 21:51:49 apb Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_inode.c,v 1.2 2011/07/18 21:51:49 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/param.h>
#else
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_endian.h"
#include "v7fs_inode.h"
#include "v7fs_superblock.h"

#ifdef V7FS_INODE_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

static void v7fs_inode_setup_disk_image(const struct v7fs_self *,
    struct v7fs_inode *, struct v7fs_inode_diskimage *);
static int v7fs_inode_inquire_disk_location(const struct v7fs_self *,
    v7fs_ino_t, v7fs_daddr_t *, v7fs_daddr_t *);
#ifdef V7FS_INODE_DEBUG
static int v7fs_inode_block_sanity(const struct v7fs_superblock *,
    v7fs_daddr_t);

static int
v7fs_inode_block_sanity(const struct v7fs_superblock *sb, v7fs_daddr_t blk)
{

	if ((blk < V7FS_ILIST_SECTOR) || (blk >= sb->datablock_start_sector)) {
		DPRINTF("invalid inode block#%d (%d-%d)\n", blk,
		    V7FS_ILIST_SECTOR, sb->datablock_start_sector);
		return ENOSPC;
	}

	return 0;
}
#endif /* V7FS_INODE_DEBUG */

int
v7fs_inode_number_sanity(const struct v7fs_superblock *sb, v7fs_ino_t ino)
{

	if (ino < V7FS_ROOT_INODE || ((size_t)ino >= V7FS_MAX_INODE(sb))) {
		DPRINTF("invalid inode#%d (%d-%zu)\n", ino,
		    V7FS_ROOT_INODE, V7FS_MAX_INODE(sb));
		return ENOSPC;
	}

	return 0;
}

int
v7fs_inode_allocate(struct v7fs_self *fs, v7fs_ino_t *ino)
{
	struct v7fs_superblock *sb = &fs->superblock;
	v7fs_ino_t inode_number;
	int error = ENOSPC;
	*ino = 0;

	SUPERB_LOCK(fs);
	if (sb->total_freeinode == 0) {
		DPRINTF("inode exhausted!(1)\n");
		goto errexit;
	}

	/* If there is no free inode cache, update it. */
	if (sb->nfreeinode <= 0 && (error = v7fs_freeinode_update(fs))) {
		DPRINTF("inode exhausted!(2)\n");
		goto errexit;
	}
	/* Get inode from superblock cache. */
	KDASSERT(sb->nfreeinode <= V7FS_MAX_FREEINODE);
	inode_number = sb->freeinode[--sb->nfreeinode];
	sb->total_freeinode--;
	sb->modified = 1;

	if ((error = v7fs_inode_number_sanity(sb, inode_number))) {
		DPRINTF("new inode#%d %d %d\n", inode_number, sb->nfreeinode,
		    sb->total_freeinode);
		DPRINTF("free inode list corupt\n");
		goto errexit;
	}
	*ino = inode_number;

errexit:
	SUPERB_UNLOCK(fs);

	return error;
}

void
v7fs_inode_deallocate(struct v7fs_self *fs, v7fs_ino_t ino)
{
	struct v7fs_superblock *sb = &fs->superblock;
	struct v7fs_inode inode;

	memset(&inode, 0, sizeof(inode));
	inode.inode_number = ino;
	v7fs_inode_writeback(fs, &inode);

	SUPERB_LOCK(fs);
	if (sb->nfreeinode < V7FS_MAX_FREEINODE) {
		/* link to freeinode list. */
		sb->freeinode[sb->nfreeinode++] = ino;
	}
	/* If superblock inode cache is full, this inode charged by
	   v7fs_freeinode_update() later. */
	sb->total_freeinode++;
	sb->modified = true;
	SUPERB_UNLOCK(fs);
}

void
v7fs_inode_setup_memory_image(const struct v7fs_self *fs __unused,
    struct v7fs_inode *mem, struct v7fs_inode_diskimage *disk)
{
#define	conv16(m)	(mem->m = V7FS_VAL16(fs, (disk->m)))
#define	conv32(m)	(mem->m = V7FS_VAL32(fs, (disk->m)))
	uint32_t addr;
	int i;

	memset(mem, 0, sizeof(*mem));
	conv16(mode);
	conv16(nlink);
	conv16(uid);
	conv16(gid);
	conv32(filesize);
	conv32(atime);
	conv32(mtime);
	conv32(ctime);

	for (i = 0; i < V7FS_NADDR; i++) {
		int j = i * 3; /* 3 byte each. (v7fs_daddr is 24bit) */
		/* expand to 4byte with endian conversion. */
		addr = V7FS_VAL24_READ(fs, &disk->addr[j]);
		mem->addr[i] = addr;
	}
	mem->device = 0;
	if (v7fs_inode_iscdev(mem) || v7fs_inode_isbdev(mem)) {
		mem->device = mem->addr[0];
	}

#undef conv16
#undef conv32
}

static void
v7fs_inode_setup_disk_image(const struct v7fs_self *fs __unused,
    struct v7fs_inode *mem, struct v7fs_inode_diskimage *disk)
{
#define	conv16(m)	(disk->m = V7FS_VAL16(fs, (mem->m)))
#define	conv32(m)	(disk->m = V7FS_VAL32(fs, (mem->m)))

	conv16(mode);
	conv16(nlink);
	conv16(uid);
	conv16(gid);
	conv32(filesize);
	conv32(atime);
	conv32(mtime);
	conv32(ctime);

	int i;
	for (i = 0; i < V7FS_NADDR; i++) {
		int j = i * 3; /* 3 byte each. */
		V7FS_VAL24_WRITE(fs, mem->addr[i], disk->addr + j);
	}
#undef conv16
#undef conv32
}

/* Load inode from disk. */
int
v7fs_inode_load(struct v7fs_self *fs, struct v7fs_inode *p, v7fs_ino_t n)
{
	v7fs_daddr_t blk, ofs;
	struct v7fs_inode_diskimage *di;
	void *buf;

	if (v7fs_inode_inquire_disk_location(fs, n, &blk, &ofs) != 0)
		return ENOENT;

	ILIST_LOCK(fs);
	if (!(buf = scratch_read(fs, blk))) {
		ILIST_UNLOCK(fs);
		return EIO;
	}
	ILIST_UNLOCK(fs);
	di = (struct v7fs_inode_diskimage *)buf;

	/* Decode disk address, convert endian. */
	v7fs_inode_setup_memory_image(fs, p, di + ofs);
	p->inode_number = n;

	scratch_free(fs, buf);

	return 0;
}

/* Write back inode to disk. */
int
v7fs_inode_writeback(struct v7fs_self *fs, struct v7fs_inode *mem)
{
	struct v7fs_inode_diskimage disk;
	v7fs_ino_t ino = mem->inode_number;
	v7fs_daddr_t blk;
	v7fs_daddr_t ofs;
	void *buf;
	int error = 0;

	if (v7fs_inode_inquire_disk_location(fs, ino, &blk, &ofs) != 0)
		return ENOENT;

	v7fs_inode_setup_disk_image(fs, mem, &disk);

	ILIST_LOCK(fs);
	if (!(buf = scratch_read(fs, blk))) {
		ILIST_UNLOCK(fs);
		return EIO;
	}
	struct v7fs_inode_diskimage *di = (struct v7fs_inode_diskimage *)buf;
	di[ofs] = disk; /* structure copy; */
	if (!fs->io.write(fs->io.cookie, buf, blk))
		error = EIO;
	ILIST_UNLOCK(fs);

	scratch_free(fs, buf);

	return error;
}

static int
v7fs_inode_inquire_disk_location(const struct v7fs_self *fs
    __unused, v7fs_ino_t n, v7fs_daddr_t *block,
    v7fs_daddr_t *offset)
{
	v7fs_daddr_t ofs, blk;
#ifdef V7FS_INODE_DEBUG
	v7fs_inode_number_sanity(&fs->superblock, n);
#endif
	ofs = (n - 1/*inode start from 1*/) *
	    sizeof(struct v7fs_inode_diskimage);
	blk = ofs >> V7FS_BSHIFT;

	*block = blk + V7FS_ILIST_SECTOR;
	*offset = (ofs - blk * V7FS_BSIZE) /
	    sizeof(struct v7fs_inode_diskimage);
#ifdef V7FS_INODE_DEBUG
	return v7fs_inode_block_sanity(&fs->superblock, *block);
#else
	return 0;
#endif
}

