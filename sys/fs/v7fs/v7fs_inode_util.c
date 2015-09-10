/*	$NetBSD: v7fs_inode_util.c,v 1.2 2011/07/18 21:51:49 apb Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_inode_util.c,v 1.2 2011/07/18 21:51:49 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/param.h>
#else
#include <stdio.h>
#include <time.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_inode.h"

#ifdef V7FS_INODE_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

void
v7fs_inode_chmod(struct v7fs_inode *inode, v7fs_mode_t mode)
{
#define	V7FS_MODE_MASK	0xfff
	DPRINTF("setattr %08o -> %08o\n", inode->mode, mode);

	inode->mode &= ~V7FS_MODE_MASK;
	inode->mode |= (mode & V7FS_MODE_MASK);
	DPRINTF("setattr %08o -> %08o\n", inode->mode, mode);
}

void
v7fs_inode_dump(const struct v7fs_inode *p)
{
	printf("nlink %d mode %06o  %d/%d %d bytes\n",
	    p->nlink, p->mode,
	    p->uid, p->gid, p->filesize);

	printf("atime %d mtime %d ctime %d\n",
	    p->atime, p->mtime, p->ctime);
#ifndef _KERNEL
	time_t at = p->atime;
	time_t mt = p->mtime;
	time_t ct = p->ctime;
	printf(" atime %s mtime %s ctime %s", ctime(&at), ctime(&mt),
	    ctime(&ct));
#endif
	if (v7fs_inode_iscdev(p) || v7fs_inode_isbdev(p)) {
		printf("device:%d/%d\n", (p->device >> 8), p->device & 0xff);
	}
	printf("\n");
}


int
v7fs_ilist_foreach
(struct v7fs_self *fs,
    int (*func)(struct v7fs_self *, void *, struct v7fs_inode *, v7fs_ino_t),
    void *ctx)
{
	struct v7fs_superblock *sb = &fs->superblock;
	size_t i, j, k;
	int ret;

	/* Loop over ilist. */
	for (k = 1, i = V7FS_ILIST_SECTOR; i < sb->datablock_start_sector;
	    i++) {
		struct v7fs_inode_diskimage *di;
		struct v7fs_inode inode;
		void *buf;

		if (!(buf = scratch_read(fs, i))) {
			DPRINTF("block %zu I/O error.\n", i);
			k += V7FS_INODE_PER_BLOCK;
			continue;
		}
		di = (struct v7fs_inode_diskimage *)buf;
		for (j = 0; j < V7FS_INODE_PER_BLOCK; j++, k++) {
			v7fs_inode_setup_memory_image(fs, &inode, di + j);
			inode.inode_number = k;
			if ((ret = func(fs, ctx, &inode, k))) {
				scratch_free(fs, buf);
				return ret;
			}
		}
		scratch_free(fs, buf);
	}
	return 0;
}
