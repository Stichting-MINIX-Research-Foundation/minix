/*	$NetBSD: v7fs_file_util.c,v 1.4 2011/07/30 03:52:04 uch Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: v7fs_file_util.c,v 1.4 2011/07/30 03:52:04 uch Exp $");
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
#include "v7fs_dirent.h"
#include "v7fs_file.h"
#include "v7fs_datablock.h"

#ifdef V7FS_FILE_DEBUG
#define	DPRINTF(fmt, args...)	printf("%s: " fmt, __func__, ##args)
#else
#define	DPRINTF(fmt, args...)	((void)0)
#endif

static int replace_subr(struct v7fs_self *, void *, v7fs_daddr_t, size_t);
static int lookup_by_number_subr(struct v7fs_self *, void *, v7fs_daddr_t,
    size_t);
static int can_dirmove(struct v7fs_self *, v7fs_ino_t, v7fs_ino_t);
static int lookup_parent_from_dir_subr(struct v7fs_self *, void *,
    v7fs_daddr_t, size_t);

int
v7fs_file_link(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    struct v7fs_inode *p, const char *name)
{
	int error = 0;

	DPRINTF("%d %d %s\n", parent_dir->inode_number, p->inode_number, name);
	if ((error = v7fs_directory_add_entry(fs, parent_dir, p->inode_number,
	    name))) {
		DPRINTF("can't add entry");
		return error;
	}
	p->nlink++;
	v7fs_inode_writeback(fs, p);

	return 0;
}

int
v7fs_file_symlink(struct v7fs_self *fs, struct v7fs_inode *p,
    const char *target)
{
	int error;
	size_t len = strlen(target) + 1;

	if (len > V7FSBSD_MAXSYMLINKLEN) {/* limited target 512byte pathname */
		DPRINTF("too long pathname.");
		return ENAMETOOLONG;
	}

	if ((error = v7fs_datablock_expand(fs, p, len))) {
		return error;
	}

	v7fs_daddr_t blk = p->addr[0];	/* 1block only.  */
	void *buf;
	if (!(buf = scratch_read(fs, blk))) {
		return EIO;
	}

	strncpy(buf, target, V7FS_BSIZE);
	if (!fs->io.write(fs->io.cookie, buf, blk)) {
		scratch_free(fs, buf);
		return EIO;
	}
	scratch_free(fs, buf);
	v7fs_inode_writeback(fs, p);

	return 0;
}

int
v7fs_file_rename(struct v7fs_self *fs, struct v7fs_inode *parent_from,
    const char *from, struct v7fs_inode *parent_to, const char *to)
{
	v7fs_ino_t from_ino, to_ino;
	struct v7fs_inode inode;
	int error;
	bool dir_move;

	/* Check source file */
	if ((error = v7fs_file_lookup_by_name(fs, parent_from, from,
	    &from_ino))) {
		DPRINTF("%s don't exists\n", from);
		return error;
	}
	v7fs_inode_load(fs, &inode, from_ino);
	dir_move = v7fs_inode_isdir(&inode);

	/* Check target file */
	error = v7fs_file_lookup_by_name(fs, parent_to, to, &to_ino);
	if (error == 0) {	/* found */
		DPRINTF("%s already exists\n", to);
		if ((error = v7fs_file_deallocate(fs, parent_to, to))) {
			DPRINTF("%s can't remove %d\n", to, error);
			return error;
		}
	} else if (error != ENOENT) {
		DPRINTF("error=%d\n", error);
		return error;
	}
	/* Check directory hierarchy. t_vnops rename_dir(5) */
	if (dir_move && (error = can_dirmove(fs, from_ino,
	    parent_to->inode_number))) {
		DPRINTF("dst '%s' is child dir of '%s'. error=%d\n", to, from,
		    error);
		return error;
	}

	if ((error = v7fs_directory_add_entry(fs, parent_to, from_ino, to))) {
		DPRINTF("can't add entry");
		return error;
	}

	if ((error = v7fs_directory_remove_entry(fs, parent_from, from))) {
		DPRINTF("can't remove entry");
		return error;
	}

	if (dir_move && (parent_from != parent_to)) {
		/* If directory move, update ".." */
		if ((error = v7fs_directory_replace_entry(fs, &inode, "..",
			    parent_to->inode_number))) {
			DPRINTF("can't replace parent dir");
			return error;
		}
		v7fs_inode_writeback(fs, &inode);
	}

	return 0;
}


int
v7fs_directory_replace_entry(struct v7fs_self *fs,  struct v7fs_inode *self_dir,
    const char *name, v7fs_ino_t ino)
{
	int error;

	/* Search entry that replaced. replace it to new inode number. */
	struct v7fs_lookup_arg lookup_arg = { .name = name,
					      .inode_number = ino };
	if ((error = v7fs_datablock_foreach(fs, self_dir, replace_subr,
	    &lookup_arg)) != V7FS_ITERATOR_BREAK)
		return ENOENT;

	return 0;
}

static int
replace_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk, size_t sz)
{
	struct v7fs_lookup_arg *p = (struct v7fs_lookup_arg *)ctx;
	struct v7fs_dirent *dir;
	void *buf;
	size_t i, n;
	int ret = 0;

	DPRINTF("match start blk=%x\n", blk);
	if (!(buf = scratch_read(fs, blk)))
		return EIO;

	dir = (struct v7fs_dirent *)buf;
	n = sz / sizeof(*dir);

	for (i = 0; i < n; i++, dir++) { /*disk endian */
		if (strncmp(p->name, (const char *)dir->name, V7FS_NAME_MAX)
		    == 0) {
			/* Replace inode# */
			dir->inode_number = V7FS_VAL16(fs, p->inode_number);
			/* Write back. */
			if (!fs->io.write(fs->io.cookie, buf, blk))
				ret = EIO;
			else
				ret = V7FS_ITERATOR_BREAK;
			break;
		}
	}
	scratch_free(fs, buf);

	return ret;
}

bool
v7fs_file_lookup_by_number(struct v7fs_self *fs, struct v7fs_inode *parent_dir,
    v7fs_ino_t ino, char *buf)
{
	int ret;

	ret = v7fs_datablock_foreach(fs, parent_dir, lookup_by_number_subr,
	    &(struct v7fs_lookup_arg){ .inode_number = ino, .buf = buf });

	return ret == V7FS_ITERATOR_BREAK;
}

static int
lookup_by_number_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk,
    size_t sz)
{
	struct v7fs_lookup_arg *p = (struct v7fs_lookup_arg *)ctx;
	struct v7fs_dirent *dir;
	void *buf;
	size_t i, n;
	int ret = 0;

	if (!(buf = scratch_read(fs, blk)))
		return EIO;

	dir = (struct v7fs_dirent *)buf;
	n = sz / sizeof(*dir);
	v7fs_dirent_endian_convert(fs, dir, n);

	for (i = 0; i < n; i++, dir++) {
		if (dir->inode_number == p->inode_number) {
			if (p->buf)
				v7fs_dirent_filename(p->buf, dir->name);
			ret = V7FS_ITERATOR_BREAK;
			break;
		}
	}
	scratch_free(fs, buf);

	return ret;
}

struct lookup_parent_arg {
	v7fs_ino_t parent_ino;
};

static int
can_dirmove(struct v7fs_self *fs, v7fs_ino_t from_ino, v7fs_ino_t to_ino)
{
	struct v7fs_inode inode;
	v7fs_ino_t parent;
	int error;

	/* Start dir. */
	if ((error = v7fs_inode_load(fs, &inode, to_ino)))
		return error;

	if (!v7fs_inode_isdir(&inode))
		return ENOTDIR;

	/* Lookup the parent. */
	do {
		struct lookup_parent_arg arg;
		/* Search parent dir */
		arg.parent_ino = 0;
		v7fs_datablock_foreach(fs, &inode, lookup_parent_from_dir_subr,
		    &arg);
		if ((parent = arg.parent_ino) == 0) {
			DPRINTF("***parent missing\n");
			return ENOENT;
		}
		/* Load parent dir */
		if ((error = v7fs_inode_load(fs, &inode, parent)))
			return error;
		if (parent == from_ino) {
			DPRINTF("#%d is child dir of #%d\n", to_ino, from_ino);
			return EINVAL;
		}
	} while (parent != V7FS_ROOT_INODE);

	return 0;
}

static int
lookup_parent_from_dir_subr(struct v7fs_self *fs, void *ctx, v7fs_daddr_t blk,
    size_t sz)
{
	struct lookup_parent_arg *arg = (struct lookup_parent_arg *)ctx;
	char name[V7FS_NAME_MAX + 1];
	void *buf;
	int ret = 0;

	if (!(buf = scratch_read(fs, blk)))
		return 0;
	struct v7fs_dirent *dir = (struct v7fs_dirent *)buf;
	size_t i, n = sz / sizeof(*dir);
	if (!v7fs_dirent_endian_convert(fs, dir, n)) {
		scratch_free(fs, buf);
		return V7FS_ITERATOR_ERROR;
	}

	for (i = 0; i < n; i++, dir++) {
		v7fs_dirent_filename(name, dir->name);
		if (strncmp(dir->name, "..", V7FS_NAME_MAX) != 0)
			continue;

		arg->parent_ino = dir->inode_number;
		ret = V7FS_ITERATOR_BREAK;
		break;
	}

	scratch_free(fs, buf);
	return ret;
}
