/*	$NetBSD: ffs.c,v 1.32 2013/06/23 02:06:06 dholland Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette.
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
#if !defined(__lint)
__RCSID("$NetBSD: ffs.c,v 1.32 2013/06/23 02:06:06 dholland Exp $");
#endif	/* !__lint */

#include <sys/param.h>

#if !HAVE_NBTOOL_CONFIG_H
#include <sys/mount.h>
#endif

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

/* From <dev/raidframe/raidframevar.h> */
#define RF_PROTECTED_SECTORS 64L

#undef DIRBLKSIZ

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#ifndef NO_FFS_SWAP
#include <ufs/ufs/ufs_bswap.h>
#else
#define	ffs_sb_swap(fs_a, fs_b)
#define	ffs_dinode1_swap(inode_a, inode_b)
#define	ffs_dinode2_swap(inode_a, inode_b)
#endif

static int	ffs_match_common(ib_params *, off_t);
static int	ffs_read_disk_block(ib_params *, uint64_t, int, char []);
static int	ffs_find_disk_blocks_ufs1(ib_params *, ino_t,
		    int (*)(ib_params *, void *, uint64_t, uint32_t), void *);
static int	ffs_find_disk_blocks_ufs2(ib_params *, ino_t,
		    int (*)(ib_params *, void *, uint64_t, uint32_t), void *);
static int	ffs_findstage2_ino(ib_params *, void *, uint64_t, uint32_t);
static int	ffs_findstage2_blocks(ib_params *, void *, uint64_t, uint32_t);

static int is_ufs2;


/* This reads a disk block from the filesystem. */
static int
ffs_read_disk_block(ib_params *params, uint64_t blkno, int size, char blk[])
{
	int	rv;

	assert(params != NULL);
	assert(params->filesystem != NULL);
	assert(params->fsfd != -1);
	assert(size > 0);
	assert(blk != NULL);

	rv = pread(params->fsfd, blk, size, blkno * params->sectorsize);
	if (rv == -1) {
		warn("Reading block %llu in `%s'", 
		    (unsigned long long)blkno, params->filesystem);
		return (0);
	} else if (rv != size) {
		warnx("Reading block %llu in `%s': short read",
		    (unsigned long long)blkno, params->filesystem);
		return (0);
	}

	return (1);
}

/*
 * This iterates over the data blocks belonging to an inode,
 * making a callback each iteration with the disk block number
 * and the size.
 */
static int
ffs_find_disk_blocks_ufs1(ib_params *params, ino_t ino, 
	int (*callback)(ib_params *, void *, uint64_t, uint32_t),
	void *state)
{
	char		sbbuf[SBLOCKSIZE];
	struct fs	*fs;
	char		inodebuf[MAXBSIZE];
	struct ufs1_dinode	*inode;
	int		level_i;
	int32_t	blk, lblk, nblk;
	int		rv;
#define LEVELS 4
	struct {
		int32_t		*blknums;
		unsigned long	blkcount;
		char		diskbuf[MAXBSIZE];
	} level[LEVELS];

	assert(params != NULL);
	assert(params->fstype != NULL);
	assert(callback != NULL);
	assert(state != NULL);

	/* Read the superblock. */
	if (!ffs_read_disk_block(params, params->fstype->sblockloc, SBLOCKSIZE,
	    sbbuf))
		return (0);
	fs = (struct fs *)sbbuf;
#ifndef NO_FFS_SWAP
	if (params->fstype->needswap)
		ffs_sb_swap(fs, fs);
#endif

	if (fs->fs_inopb <= 0) {
		warnx("Bad inopb %d in superblock in `%s'",
		    fs->fs_inopb, params->filesystem);
		return (0);
	}

	/* Read the inode. */
	if (! ffs_read_disk_block(params,
		FFS_FSBTODB(fs, ino_to_fsba(fs, ino)) + params->fstype->offset,
		fs->fs_bsize, inodebuf))
		return (0);
	inode = (struct ufs1_dinode *)inodebuf;
	inode += ino_to_fsbo(fs, ino);
#ifndef NO_FFS_SWAP
	if (params->fstype->needswap)
		ffs_dinode1_swap(inode, inode);
#endif

	/* Get the block count and initialize for our block walk. */
	nblk = howmany(inode->di_size, fs->fs_bsize);
	lblk = 0;
	level_i = 0;
	level[0].blknums = &inode->di_db[0];
	level[0].blkcount = UFS_NDADDR;
	level[1].blknums = &inode->di_ib[0];
	level[1].blkcount = 1;
	level[2].blknums = &inode->di_ib[1];
	level[2].blkcount = 1;
	level[3].blknums = &inode->di_ib[2];
	level[3].blkcount = 1;

	/* Walk the data blocks. */
	while (nblk > 0) {

		/*
		 * If there are no more blocks at this indirection 
		 * level, move up one indirection level and loop.
		 */
		if (level[level_i].blkcount == 0) {
			if (++level_i == LEVELS)
				break;
			continue;
		}

		/* Get the next block at this level. */
		blk = *(level[level_i].blknums++);
		level[level_i].blkcount--;
		if (params->fstype->needswap)
			blk = bswap32(blk);

#if 0
		fprintf(stderr, "ino %lu blk %lu level %d\n", ino, blk, 
		    level_i);
#endif

		/*
		 * If we're not at the direct level, descend one
		 * level, read in that level's new block list, 
		 * and loop.
		 */
		if (level_i > 0) {
			level_i--;
			if (blk == 0)
				memset(level[level_i].diskbuf, 0, MAXBSIZE);
			else if (! ffs_read_disk_block(params, 
				FFS_FSBTODB(fs, blk) + params->fstype->offset,
				fs->fs_bsize, level[level_i].diskbuf))
				return (0);
			/* XXX ondisk32 */
			level[level_i].blknums = 
				(int32_t *)level[level_i].diskbuf;
			level[level_i].blkcount = FFS_NINDIR(fs);
			continue;
		}

		/* blk is the next direct level block. */
#if 0
		fprintf(stderr, "ino %lu db %lu blksize %lu\n", ino, 
		    FFS_FSBTODB(fs, blk), ffs_sblksize(fs, inode->di_size, lblk));
#endif
		rv = (*callback)(params, state, 
		    FFS_FSBTODB(fs, blk) + params->fstype->offset,
		    ffs_sblksize(fs, (int64_t)inode->di_size, lblk));
		lblk++;
		nblk--;
		if (rv != 1)
			return (rv);
	}

	if (nblk != 0) {
		warnx("Inode %llu in `%s' ran out of blocks?",
		    (unsigned long long)ino, params->filesystem);
		return (0);
	}

	return (1);
}

/*
 * This iterates over the data blocks belonging to an inode,
 * making a callback each iteration with the disk block number
 * and the size.
 */
static int
ffs_find_disk_blocks_ufs2(ib_params *params, ino_t ino, 
	int (*callback)(ib_params *, void *, uint64_t, uint32_t),
	void *state)
{
	char		sbbuf[SBLOCKSIZE];
	struct fs	*fs;
	char		inodebuf[MAXBSIZE];
	struct ufs2_dinode	*inode;
	int		level_i;
	int64_t		blk, lblk, nblk;
	int		rv;
#define LEVELS 4
	struct {
		int64_t		*blknums;
		unsigned long	blkcount;
		char		diskbuf[MAXBSIZE];
	} level[LEVELS];

	assert(params != NULL);
	assert(params->fstype != NULL);
	assert(callback != NULL);
	assert(state != NULL);

	/* Read the superblock. */
	if (!ffs_read_disk_block(params, params->fstype->sblockloc, SBLOCKSIZE,
	    sbbuf))
		return (0);
	fs = (struct fs *)sbbuf;
#ifndef NO_FFS_SWAP
	if (params->fstype->needswap)
		ffs_sb_swap(fs, fs);
#endif

	if (fs->fs_inopb <= 0) {
		warnx("Bad inopb %d in superblock in `%s'",
		    fs->fs_inopb, params->filesystem);
		return (0);
	}

	/* Read the inode. */
	if (! ffs_read_disk_block(params,
		FFS_FSBTODB(fs, ino_to_fsba(fs, ino)) + params->fstype->offset,
		fs->fs_bsize, inodebuf))
		return (0);
	inode = (struct ufs2_dinode *)inodebuf;
	inode += ino_to_fsbo(fs, ino);
#ifndef NO_FFS_SWAP
	if (params->fstype->needswap)
		ffs_dinode2_swap(inode, inode);
#endif

	/* Get the block count and initialize for our block walk. */
	nblk = howmany(inode->di_size, fs->fs_bsize);
	lblk = 0;
	level_i = 0;
	level[0].blknums = &inode->di_db[0];
	level[0].blkcount = UFS_NDADDR;
	level[1].blknums = &inode->di_ib[0];
	level[1].blkcount = 1;
	level[2].blknums = &inode->di_ib[1];
	level[2].blkcount = 1;
	level[3].blknums = &inode->di_ib[2];
	level[3].blkcount = 1;

	/* Walk the data blocks. */
	while (nblk > 0) {

		/*
		 * If there are no more blocks at this indirection 
		 * level, move up one indirection level and loop.
		 */
		if (level[level_i].blkcount == 0) {
			if (++level_i == LEVELS)
				break;
			continue;
		}

		/* Get the next block at this level. */
		blk = *(level[level_i].blknums++);
		level[level_i].blkcount--;
		if (params->fstype->needswap)
			blk = bswap64(blk);

#if 0
		fprintf(stderr, "ino %lu blk %llu level %d\n", ino,
		    (unsigned long long)blk, level_i);
#endif

		/*
		 * If we're not at the direct level, descend one
		 * level, read in that level's new block list, 
		 * and loop.
		 */
		if (level_i > 0) {
			level_i--;
			if (blk == 0)
				memset(level[level_i].diskbuf, 0, MAXBSIZE);
			else if (! ffs_read_disk_block(params, 
				FFS_FSBTODB(fs, blk) + params->fstype->offset,
				fs->fs_bsize, level[level_i].diskbuf))
				return (0);
			level[level_i].blknums = 
				(int64_t *)level[level_i].diskbuf;
			level[level_i].blkcount = FFS_NINDIR(fs);
			continue;
		}

		/* blk is the next direct level block. */
#if 0
		fprintf(stderr, "ino %lu db %llu blksize %lu\n", ino, 
		    FFS_FSBTODB(fs, blk), ffs_sblksize(fs, inode->di_size, lblk));
#endif
		rv = (*callback)(params, state, 
		    FFS_FSBTODB(fs, blk) + params->fstype->offset,
		    ffs_sblksize(fs, (int64_t)inode->di_size, lblk));
		lblk++;
		nblk--;
		if (rv != 1)
			return (rv);
	}

	if (nblk != 0) {
		warnx("Inode %llu in `%s' ran out of blocks?",
		    (unsigned long long)ino, params->filesystem);
		return (0);
	}

	return (1);
}

/*
 * This callback reads a block of the root directory, 
 * searches for an entry for the secondary bootstrap,
 * and saves the inode number if one is found.
 */
static int
ffs_findstage2_ino(ib_params *params, void *_ino, 
	uint64_t blk, uint32_t blksize)
{
	char		dirbuf[MAXBSIZE];
	struct direct	*de, *ede;
	uint32_t	ino;

	assert(params != NULL);
	assert(params->fstype != NULL);
	assert(params->stage2 != NULL);
	assert(_ino != NULL);

	/* Skip directory holes. */
	if (blk == 0)
		return (1);

	/* Read the directory block. */
	if (! ffs_read_disk_block(params, blk, blksize, dirbuf))
		return (0);

	/* Loop over the directory entries. */
	de = (struct direct *)&dirbuf[0];
	ede = (struct direct *)&dirbuf[blksize];
	while (de < ede) {
		ino = de->d_fileno;
		if (params->fstype->needswap) {
			ino = bswap32(ino);
			de->d_reclen = bswap16(de->d_reclen);
		}
		if (ino != 0 && strcmp(de->d_name, params->stage2) == 0) {
			*((uint32_t *)_ino) = ino;
			return (2);
		}
		if (de->d_reclen == 0)
			break;
		de = (struct direct *)((char *)de + de->d_reclen);
	}

	return (1);
}

struct findblks_state {
	uint32_t	maxblk;
	uint32_t	nblk;
	ib_block	*blocks;
};

/* This callback records the blocks of the secondary bootstrap. */
static int
ffs_findstage2_blocks(ib_params *params, void *_state,
	uint64_t blk, uint32_t blksize)
{
	struct findblks_state *state = _state;

	assert(params != NULL);
	assert(params->stage2 != NULL);
	assert(_state != NULL);

	if (state->nblk == state->maxblk) {
		warnx("Secondary bootstrap `%s' has too many blocks (max %d)",
		    params->stage2, state->maxblk);
		return (0);
	}
	state->blocks[state->nblk].block = blk;
	state->blocks[state->nblk].blocksize = blksize;
	state->nblk++;
	return (1);
}

/*
 *	publicly visible functions
 */

static off_t sblock_try[] = SBLOCKSEARCH;

int
ffs_match(ib_params *params)
{
	return ffs_match_common(params, (off_t) 0);
}

int
raid_match(ib_params *params)
{
	/* XXX Assumes 512 bytes / sector */
	if (params->sectorsize != 512) {
		warnx("Media is %d bytes/sector."
			"  RAID is only supported on 512 bytes/sector media.",
			params->sectorsize);
		return 0;
	}
	return ffs_match_common(params, (off_t) RF_PROTECTED_SECTORS);
}

int
ffs_match_common(ib_params *params, off_t offset)
{
	char		sbbuf[SBLOCKSIZE];
	struct fs	*fs;
	int i;
	off_t loc;

	assert(params != NULL);
	assert(params->fstype != NULL);

	fs = (struct fs *)sbbuf;
	for (i = 0; sblock_try[i] != -1; i++) {
		loc = sblock_try[i] / params->sectorsize + offset;
		if (!ffs_read_disk_block(params, loc, SBLOCKSIZE, sbbuf))
			continue;
		switch (fs->fs_magic) {
		case FS_UFS2_MAGIC:
			is_ufs2 = 1;
			/* FALLTHROUGH */
		case FS_UFS1_MAGIC:
			params->fstype->needswap = 0;
			params->fstype->blocksize = fs->fs_bsize;
			params->fstype->sblockloc = loc;
			params->fstype->offset = offset;
			break;
#ifndef FFS_NO_SWAP
		case FS_UFS2_MAGIC_SWAPPED:
			is_ufs2 = 1;
			/* FALLTHROUGH */
		case FS_UFS1_MAGIC_SWAPPED:
			params->fstype->needswap = 1;
			params->fstype->blocksize = bswap32(fs->fs_bsize);
			params->fstype->sblockloc = loc;
			params->fstype->offset = offset;
			break;
#endif
		default:
			continue;
		}
		if (!is_ufs2 && sblock_try[i] == SBLOCK_UFS2)
			continue;
		return 1;
	}

	return (0);
}

int
ffs_findstage2(ib_params *params, uint32_t *maxblk, ib_block *blocks)
{
	int			rv;
	uint32_t		ino;
	struct findblks_state	state;

	assert(params != NULL);
	assert(params->stage2 != NULL);
	assert(maxblk != NULL);
	assert(blocks != NULL);

	if (params->flags & IB_STAGE2START)
		return (hardcode_stage2(params, maxblk, blocks));

	/* The secondary bootstrap must be clearly in /. */
	if (params->stage2[0] == '/')
		params->stage2++;
	if (strchr(params->stage2, '/') != NULL) {
		warnx("The secondary bootstrap `%s' must be in /",
		    params->stage2);
		warnx("(Path must be relative to the file system in `%s')",
		    params->filesystem);
		return (0);
	}

	/* Get the inode number of the secondary bootstrap. */
	if (is_ufs2)
		rv = ffs_find_disk_blocks_ufs2(params, UFS_ROOTINO,
		    ffs_findstage2_ino, &ino);
	else
		rv = ffs_find_disk_blocks_ufs1(params, UFS_ROOTINO,
		    ffs_findstage2_ino, &ino);
	if (rv != 2) {
		warnx("Could not find secondary bootstrap `%s' in `%s'",
		    params->stage2, params->filesystem);
		warnx("(Path must be relative to the file system in `%s')",
		    params->filesystem);
		return (0);
	}

	/* Record the disk blocks of the secondary bootstrap. */
	state.maxblk = *maxblk;
	state.nblk = 0;
	state.blocks = blocks;
	if (is_ufs2)
		rv = ffs_find_disk_blocks_ufs2(params, ino,
		    ffs_findstage2_blocks, &state);
	else
		rv = ffs_find_disk_blocks_ufs1(params, ino,
		    ffs_findstage2_blocks, &state);
	if (! rv) {
		return (0);
	}

	*maxblk = state.nblk;
	return (1);
}
