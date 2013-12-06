/*	$NetBSD: ext2fs.c,v 1.9 2013/06/23 02:06:06 dholland Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 */

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
__RCSID("$NetBSD: ext2fs.c,v 1.9 2013/06/23 02:06:06 dholland Exp $");
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

#include <ufs/ext2fs/ext2fs_dinode.h>
#include <ufs/ext2fs/ext2fs_dir.h>
#include <ufs/ext2fs/ext2fs.h>

static int	ext2fs_read_disk_block(ib_params *, uint64_t, int, uint8_t []);
static int	ext2fs_read_sblock(ib_params *, struct m_ext2fs *fs);
static int	ext2fs_read_gdblock(ib_params *, struct m_ext2fs *fs);
static int	ext2fs_find_disk_blocks(ib_params *, ino_t,
		    int (*)(ib_params *, void *, uint64_t, uint32_t), void *);
static int	ext2fs_findstage2_ino(ib_params *, void *, uint64_t, uint32_t);
static int	ext2fs_findstage2_blocks(ib_params *, void *, uint64_t,
		    uint32_t);


/* This reads a disk block from the file system. */
/* XXX: should be shared with ffs.c? */
static int
ext2fs_read_disk_block(ib_params *params, uint64_t blkno, int size,
    uint8_t blk[])
{
	int rv;

	assert(params != NULL);
	assert(params->filesystem != NULL);
	assert(params->fsfd != -1);
	assert(size > 0);
	assert(blk != NULL);

	rv = pread(params->fsfd, blk, size, blkno * params->sectorsize);
	if (rv == -1) {
		warn("Reading block %llu in `%s'", 
		    (unsigned long long)blkno, params->filesystem);
		return 0;
	} else if (rv != size) {
		warnx("Reading block %llu in `%s': short read",
		    (unsigned long long)blkno, params->filesystem);
		return 0;
	}

	return 1;
}

static int
ext2fs_read_sblock(ib_params *params, struct m_ext2fs *fs)
{
	uint8_t sbbuf[SBSIZE];

	if (ext2fs_read_disk_block(params, SBOFF / params->sectorsize, SBSIZE,
	    sbbuf) == 0)

	e2fs_sbload((void *)sbbuf, &fs->e2fs);

	if (fs->e2fs.e2fs_magic != E2FS_MAGIC)
		return 0;

	if (fs->e2fs.e2fs_rev > E2FS_REV1 ||
	    (fs->e2fs.e2fs_rev == E2FS_REV1 &&
	     (fs->e2fs.e2fs_first_ino != EXT2_FIRSTINO ||
	      fs->e2fs.e2fs_inode_size != EXT2_DINODE_SIZE ||
	      (fs->e2fs.e2fs_features_incompat & ~EXT2F_INCOMPAT_SUPP) != 0)))
		return 0;

	fs->e2fs_ncg =
	    howmany(fs->e2fs.e2fs_bcount - fs->e2fs.e2fs_first_dblock,
	    fs->e2fs.e2fs_bpg);
	/* XXX assume hw bsize = 512 */
	fs->e2fs_fsbtodb = fs->e2fs.e2fs_log_bsize + 1;
	fs->e2fs_bsize = MINBSIZE << fs->e2fs.e2fs_log_bsize;
	fs->e2fs_bshift = LOG_MINBSIZE + fs->e2fs.e2fs_log_bsize;
	fs->e2fs_qbmask = fs->e2fs_bsize - 1;
	fs->e2fs_bmask = ~fs->e2fs_qbmask;
	fs->e2fs_ngdb =
	    howmany(fs->e2fs_ncg, fs->e2fs_bsize / sizeof(struct ext2_gd));
	fs->e2fs_ipb = fs->e2fs_bsize / EXT2_DINODE_SIZE;
	fs->e2fs_itpg = fs->e2fs.e2fs_ipg / fs->e2fs_ipb;

	return 1;
}

static int
ext2fs_read_gdblock(ib_params *params, struct m_ext2fs *fs)
{
	uint8_t gdbuf[MAXBSIZE];
	uint32_t gdpb;
	int i;

	gdpb = fs->e2fs_bsize / sizeof(struct ext2_gd);

	for (i = 0; i < fs->e2fs_ngdb; i++) {
		if (ext2fs_read_disk_block(params, EXT2_FSBTODB(fs,
		    fs->e2fs.e2fs_first_dblock + 1 /* superblock */ + i),
		    SBSIZE, gdbuf) == 0)
			return 0;

		e2fs_cgload((struct ext2_gd *)gdbuf, &fs->e2fs_gd[gdpb * i],
		    (i == (fs->e2fs_ngdb - 1)) ?
		    (fs->e2fs_ncg - gdpb * i) * sizeof(struct ext2_gd):
		    fs->e2fs_bsize);
	}

	return 1;
}

/*
 * This iterates over the data blocks belonging to an inode,
 * making a callback each iteration with the disk block number
 * and the size.
 */
static int
ext2fs_find_disk_blocks(ib_params *params, ino_t ino, 
	int (*callback)(ib_params *, void *, uint64_t, uint32_t),
	void *state)
{
	uint8_t sbbuf[sizeof(struct m_ext2fs)];
	struct m_ext2fs *fs;
	uint8_t inodebuf[MAXBSIZE];
	struct ext2fs_dinode inode_store, *inode;
	int level_i;
	int32_t blk, lblk, nblk;
	int rv;
#define LEVELS 4
	struct {
		uint32_t *blknums;
		unsigned long blkcount;
		uint8_t diskbuf[MAXBSIZE];
	} level[LEVELS];

	assert(params != NULL);
	assert(params->fstype != NULL);
	assert(callback != NULL);
	assert(state != NULL);

	/* Read the superblock. */
	fs = (void *)sbbuf;
	if (ext2fs_read_sblock(params, fs) == 0)
		return 0;

	fs->e2fs_gd = malloc(sizeof(struct ext2_gd) * fs->e2fs_ncg);
	if (fs->e2fs_gd == NULL) {
		warnx("Can't allocate memofy for group descriptors");
		return 0;
	}

	if (ext2fs_read_gdblock(params, fs) == 0) {
		warnx("Can't read group descriptors");
		return 0;
	}

	if (fs->e2fs_ipb <= 0) {
		warnx("Bad ipb %d in superblock in `%s'",
		    fs->e2fs_ipb, params->filesystem);
		return 0;
	}

	/* Read the inode. */
	if (ext2fs_read_disk_block(params,
		EXT2_FSBTODB(fs, ino_to_fsba(fs, ino)) + params->fstype->offset,
		fs->e2fs_bsize, inodebuf))
		return 0;
	inode = (void *)inodebuf;
	e2fs_iload(&inode[ino_to_fsbo(fs, ino)], &inode_store);
	inode = &inode_store;

	/* Get the block count and initialize for our block walk. */
	nblk = howmany(inode->e2di_size, fs->e2fs_bsize);
	lblk = 0;
	level_i = 0;
	level[0].blknums = &inode->e2di_blocks[0];
	level[0].blkcount = UFS_NDADDR;
	level[1].blknums = &inode->e2di_blocks[UFS_NDADDR + 0];
	level[1].blkcount = 1;
	level[2].blknums = &inode->e2di_blocks[UFS_NDADDR + 1];
	level[2].blkcount = 1;
	level[3].blknums = &inode->e2di_blocks[UFS_NDADDR + 2];
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
		blk = fs2h32(*(level[level_i].blknums++));
		level[level_i].blkcount--;

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
			else if (ext2fs_read_disk_block(params, 
				EXT2_FSBTODB(fs, blk) + params->fstype->offset,
				fs->e2fs_bsize, level[level_i].diskbuf) == 0)
				return 0;
			/* XXX ondisk32 */
			level[level_i].blknums = 
			    (uint32_t *)level[level_i].diskbuf;
			level[level_i].blkcount = EXT2_NINDIR(fs);
			continue;
		}

		/* blk is the next direct level block. */
#if 0
		fprintf(stderr, "ino %lu db %lu blksize %lu\n", ino, 
		    EXT2_FSBTODB(fs, blk), ext2_sblksize(fs, inode->di_size, lblk));
#endif
		rv = (*callback)(params, state, 
		    EXT2_FSBTODB(fs, blk) + params->fstype->offset, fs->e2fs_bsize);
		lblk++;
		nblk--;
		if (rv != 1)
			return rv;
	}

	if (nblk != 0) {
		warnx("Inode %llu in `%s' ran out of blocks?",
		    (unsigned long long)ino, params->filesystem);
		return 0;
	}

	return 1;
}

/*
 * This callback reads a block of the root directory, 
 * searches for an entry for the secondary bootstrap,
 * and saves the inode number if one is found.
 */
static int
ext2fs_findstage2_ino(ib_params *params, void *_ino, 
	uint64_t blk, uint32_t blksize)
{
	uint8_t dirbuf[MAXBSIZE];
	struct ext2fs_direct *de, *ede;
	uint32_t ino;

	assert(params != NULL);
	assert(params->fstype != NULL);
	assert(params->stage2 != NULL);
	assert(_ino != NULL);

	/* Skip directory holes. */
	if (blk == 0)
		return 1;

	/* Read the directory block. */
	if (ext2fs_read_disk_block(params, blk, blksize, dirbuf) == 0)
		return 0;

	/* Loop over the directory entries. */
	de = (struct ext2fs_direct *)&dirbuf[0];
	ede = (struct ext2fs_direct *)&dirbuf[blksize];
	while (de < ede) {
		ino = fs2h32(de->e2d_ino);
		if (ino != 0 && strcmp(de->e2d_name, params->stage2) == 0) {
			*((uint32_t *)_ino) = ino;
			return (2);
		}
		if (fs2h16(de->e2d_reclen) == 0)
			break;
		de = (struct ext2fs_direct *)((char *)de +
		    fs2h16(de->e2d_reclen));
	}

	return 1;
}

struct findblks_state {
	uint32_t	maxblk;
	uint32_t	nblk;
	ib_block	*blocks;
};

/* This callback records the blocks of the secondary bootstrap. */
static int
ext2fs_findstage2_blocks(ib_params *params, void *_state,
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
	return 1;
}

/*
 *	publicly visible functions
 */

int
ext2fs_match(ib_params *params)
{
	uint8_t sbbuf[sizeof(struct m_ext2fs)];
	struct m_ext2fs *fs;

	assert(params != NULL);
	assert(params->fstype != NULL);

	/* Read the superblock. */
	fs = (void *)sbbuf;
	if (ext2fs_read_sblock(params, fs) == 0)
		return 0;

	params->fstype->needswap = 0;
	params->fstype->blocksize = fs->e2fs_bsize;
	params->fstype->offset = 0;

	return 1;
}

int
ext2fs_findstage2(ib_params *params, uint32_t *maxblk, ib_block *blocks)
{
	int rv;
	uint32_t ino;
	struct findblks_state state;

	assert(params != NULL);
	assert(params->stage2 != NULL);
	assert(maxblk != NULL);
	assert(blocks != NULL);

	if (params->flags & IB_STAGE2START)
		return hardcode_stage2(params, maxblk, blocks);

	/* The secondary bootstrap must be clearly in /. */
	if (params->stage2[0] == '/')
		params->stage2++;
	if (strchr(params->stage2, '/') != NULL) {
		warnx("The secondary bootstrap `%s' must be in /",
		    params->stage2);
		warnx("(Path must be relative to the file system in `%s')",
		    params->filesystem);
		return 0;
	}

	/* Get the inode number of the secondary bootstrap. */
	rv = ext2fs_find_disk_blocks(params, EXT2_ROOTINO,
	    ext2fs_findstage2_ino, &ino);
	if (rv != 2) {
		warnx("Could not find secondary bootstrap `%s' in `%s'",
		    params->stage2, params->filesystem);
		warnx("(Path must be relative to the file system in `%s')",
		    params->filesystem);
		return 0;
	}

	/* Record the disk blocks of the secondary bootstrap. */
	state.maxblk = *maxblk;
	state.nblk = 0;
	state.blocks = blocks;
		rv = ext2fs_find_disk_blocks(params, ino,
		    ext2fs_findstage2_blocks, &state);
	if (rv == 0)
		return 0;

	*maxblk = state.nblk;
	return 1;
}
