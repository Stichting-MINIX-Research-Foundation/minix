/*	$NetBSD: main.c,v 1.10 2011/08/10 11:31:49 uch Exp $	*/

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
#ifndef lint
__RCSID("$NetBSD: main.c,v 1.10 2011/08/10 11:31:49 uch Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <err.h>

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_endian.h"
#include "v7fs_superblock.h"
#include "v7fs_inode.h"
#include "v7fs_datablock.h" /*v7fs_datablock_expand/last */
#include "newfs_v7fs.h"
#include "progress.h" /*../sbin/fsck */

#define	VPRINTF(lv, fmt, args...)	{ if (v7fs_newfs_verbose >= lv)	\
	printf(fmt, ##args); }

static v7fs_daddr_t
determine_ilist_size(v7fs_daddr_t volume_size, int32_t files)
{
	v7fs_daddr_t ilist_size;

	if (files)
		ilist_size =  howmany(files, V7FS_INODE_PER_BLOCK);
	else
		ilist_size = volume_size / 25; /* 4% */
	if (ilist_size > (v7fs_daddr_t)V7FS_ILISTBLK_MAX)
		ilist_size = V7FS_ILISTBLK_MAX;

	return ilist_size;
}

static int
partition_check(struct v7fs_self *fs)
{
	struct v7fs_superblock *sb = &fs->superblock;
	int error;

	if ((error = v7fs_superblock_load(fs))) {
		if (error != EINVAL) {
			/* Invalid superblock information is OK. */
			warnx("Can't read superblock sector.");
		}
	}
	sb->modified = 1;
	if ((error = v7fs_superblock_writeback(fs))) {
		if (errno == EROFS) {
			warnx("Overwriting disk label? ");
		}
		warnx("Can't write superblock sector.");
	}

	return error;
}

static int
make_root(struct v7fs_self *fs)
{
	struct v7fs_inode inode;
	struct v7fs_dirent *dir;
	int error;

	/* INO 1 badblk (don't used) */
	memset(&inode, 0, sizeof(inode));
	inode.inode_number = 1;
	inode.mode = V7FS_IFREG;	/* V7 manner */
	v7fs_inode_writeback(fs, &inode);

	/* INO 2 root */
	v7fs_ino_t ino;
	if ((error = v7fs_inode_allocate(fs, &ino))) {
		errno = error;
		warn("Can't allocate / inode");
		return error;
	}

	memset(&inode, 0, sizeof(inode));
	inode.inode_number = ino;
	inode.mode = 0777 | V7FS_IFDIR;
	inode.uid = 0;
	inode.gid = 0;
	inode.nlink = 2;	/* . + .. */
	inode.atime = inode.mtime = inode.ctime = time(0);

	/* root dirent. */
	v7fs_datablock_expand(fs, &inode, sizeof(*dir) * 2);
	v7fs_daddr_t blk = inode.addr[0];
	void *buf;
	if (!(buf = scratch_read(fs, blk))) {
		v7fs_inode_deallocate(fs, ino);
		errno = error = EIO;
		warn("Can't read / dirent.");
		return error;
	}
	dir = (struct v7fs_dirent *)buf; /*disk endian */

	strcpy(dir[0].name, ".");
	dir[0].inode_number = V7FS_VAL16(fs, ino);
	strcpy(dir[1].name, "..");
	dir[1].inode_number = V7FS_VAL16(fs, ino);
	if (!fs->io.write(fs->io.cookie, buf, blk)) {/*writeback */
		scratch_free(fs, buf);
		errno = error = EIO;
		warn("Can't write / dirent.");
		return error;
	}
	scratch_free(fs, buf);
	v7fs_inode_writeback(fs, &inode);
	if ((error = v7fs_superblock_writeback(fs))) {
		errno = error;
		warnx("Can't write superblock.");
	}

	return error;
}

static v7fs_daddr_t
make_freeblocklist(struct v7fs_self *fs, v7fs_daddr_t listblk, uint8_t *buf)
{
	uint32_t (*val32)(uint32_t) = fs->val.conv32;
	uint16_t (*val16)(uint16_t) = fs->val.conv16;
	struct v7fs_freeblock *fb = (struct v7fs_freeblock *)buf;
	int i, j, k;

	memset(buf, 0, V7FS_BSIZE);

	for (i = V7FS_MAX_FREEBLOCK - 1, j = listblk + 1, k = 0; i >= 0;
	    i--, j++, k++) {
		progress(0);
		if (j == (int32_t)fs->superblock.volume_size)
		{
			VPRINTF(4, "\nlast freeblock #%d\n",
			    (*val32)(fb->freeblock[i + 1]));

			memmove(fb->freeblock + 1, fb->freeblock + i + 1, k *
			    sizeof(v7fs_daddr_t));
			fb->freeblock[0] = 0; /* Terminate link; */
			fb->nfreeblock = (*val16)(k + 1);
			VPRINTF(4, "last freeblock contains #%d\n",
			    (*val16)(fb->nfreeblock));
			fs->io.write(fs->io.cookie, buf, listblk);
			return 0;
		}
		fb->freeblock[i] = (*val32)(j);
	}
	fb->nfreeblock = (*val16)(k);

	if (!fs->io.write(fs->io.cookie, buf, listblk)) {
		errno = EIO;
		warn("blk=%ld", (long)listblk);
		return 0;
	}

	/* Return next link block */
	return (*val32)(fb->freeblock[0]);
}

static int
make_filesystem(struct v7fs_self *fs, v7fs_daddr_t volume_size,
    v7fs_daddr_t ilist_size)
{
	struct v7fs_superblock *sb;
	v7fs_daddr_t blk;
	uint8_t buf[V7FS_BSIZE];
	int error = 0;
	int32_t i, j;

	/* Setup ilist. (ilist must be zero filled. becuase of they are free) */
	VPRINTF(4, "Zero clear ilist.\n");
	progress(&(struct progress_arg){ .label = "zero ilist", .tick =
	    ilist_size / PROGRESS_BAR_GRANULE });
	memset(buf, 0, sizeof buf);
	for (i = V7FS_ILIST_SECTOR; i < (int32_t)ilist_size; i++) {
		fs->io.write(fs->io.cookie, buf, i);
		progress(0);
	}
#ifndef HAVE_NBTOOL_CONFIG_H
	progress_done();
#endif
	VPRINTF(4, "\n");

	/* Construct superblock */
	sb = &fs->superblock;
	sb->volume_size = volume_size;
	sb->datablock_start_sector = ilist_size + V7FS_ILIST_SECTOR;
	sb->update_time = time(NULL);

	/* fill free inode cache. */
	VPRINTF(4, "Setup inode cache.\n");
	sb->nfreeinode = V7FS_MAX_FREEINODE;
	for (i = V7FS_MAX_FREEINODE - 1, j = V7FS_ROOT_INODE; i >= 0; i--, j++)
		sb->freeinode[i] = j;
	sb->total_freeinode = ilist_size * V7FS_INODE_PER_BLOCK - 1;

	/* fill free block cache. */
	VPRINTF(4, "Setup free block cache.\n");
	sb->nfreeblock = V7FS_MAX_FREEBLOCK;
	for (i = V7FS_MAX_FREEBLOCK - 1, j = sb->datablock_start_sector; i >= 0;
	    i--, j++)
		sb->freeblock[i] = j;

	sb->total_freeblock = volume_size - sb->datablock_start_sector;

	/* Write superblock. */
	sb->modified = 1;
	if ((error = v7fs_superblock_writeback(fs))) {
		errno = error;
		warn("Can't write back superblock.");
		return error;
	}

	/* Construct freeblock list */
	VPRINTF(4, "Setup whole freeblock list.\n");
	progress(&(struct progress_arg){ .label = "freeblock list", .tick =
	    (volume_size - sb->datablock_start_sector) / PROGRESS_BAR_GRANULE});
	blk = sb->freeblock[0];
	while ((blk = make_freeblocklist(fs, blk, buf)))
		continue;
#ifndef HAVE_NBTOOL_CONFIG_H
	progress_done();
#endif

	VPRINTF(4, "done.\n");

	return 0;
}

int
v7fs_newfs(const struct v7fs_mount_device *mount, int32_t maxfile)
{
	struct v7fs_self *fs;
	v7fs_daddr_t ilist_size;
	int error;
	v7fs_daddr_t volume_size = mount->sectors;

	/* Check and determine ilistblock, datablock size. */
	if (volume_size > V7FS_DADDR_MAX + 1) {
		warnx("volume size %d over v7fs limit %d. truncated.",
		    volume_size, V7FS_DADDR_MAX + 1);
		volume_size = V7FS_DADDR_MAX + 1;
	}

	ilist_size = determine_ilist_size(volume_size, maxfile);

	VPRINTF(1, "volume size=%d, ilist size=%d, endian=%d, NAME_MAX=%d\n",
	    volume_size, ilist_size, mount->endian, V7FS_NAME_MAX);

	/* Setup I/O ops. */
	if ((error = v7fs_io_init(&fs, mount, V7FS_BSIZE))) {
		errno = error;
		warn("I/O setup failed.");
		return error;
	}
	fs->endian = mount->endian;
	v7fs_endian_init(fs);

	if ((error = partition_check(fs))) {
		return error;
	}

	/* Construct filesystem. */
	if ((error = make_filesystem(fs, volume_size, ilist_size))) {
		return error;
	}

	/* Setup root. */
	if ((error = make_root(fs))) {
		return error;
	}

	v7fs_io_fini(fs);

	return 0;
}
