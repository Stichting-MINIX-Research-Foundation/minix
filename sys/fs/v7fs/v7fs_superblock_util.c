/*	$NetBSD: v7fs_superblock_util.c,v 1.2 2011/07/18 21:51:49 apb Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_superblock_util.c,v 1.2 2011/07/18 21:51:49 apb Exp $");
#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/param.h>	/* errno */
#else
#include <stdio.h>
#include <time.h>
#endif

#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_superblock.h"
#include "v7fs_inode.h"

#ifdef V7FS_SUPERBLOCK_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#define	DPRINTF_(fmt, args...)	printf(fmt, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#define	DPRINTF_(fmt, args...)	((void)0)
#endif

void
v7fs_superblock_status(struct v7fs_self *fs)
{
	struct v7fs_superblock *sb = &fs->superblock;
	struct v7fs_stat *stat = &fs->stat;

	stat->total_blocks = sb->volume_size - sb->datablock_start_sector;
	stat->total_inode = V7FS_MAX_INODE(sb);
	stat->free_inode = sb->total_freeinode;
	stat->free_blocks = sb->total_freeblock;
	stat->total_files = stat->total_inode - sb->total_freeinode - 1;

	DPRINTF("block %d/%d, inode %d/%d\n", stat->free_blocks,
	    stat->total_blocks, stat->free_inode, stat->total_inode);
}

void
v7fs_superblock_dump(const struct v7fs_self *fs)
{
	const struct v7fs_superblock *sb = &fs->superblock;

#define	print(x)	printf("%s: %d\n", #x, sb->x)
	print(datablock_start_sector);
	print(volume_size);
	print(nfreeblock);
	print(nfreeinode);
	print(update_time);
	print(lock_freeblock);
	print(lock_freeinode);
	print(modified);
	print(readonly);
#if !defined _KERNEL
	time_t t = sb->update_time;
	printf("%s", ctime(&t));
#endif
	print(total_freeblock);
	print(total_freeinode);
#undef print
}
